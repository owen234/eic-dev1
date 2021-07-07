#include "Pythia8/Pythia.h"
#include "Pythia8Plugins/HepMC3.h"
#include <cassert>
#include <unistd.h>

using namespace Pythia8;

#include "clipp.h"
using namespace clipp;
using std::string;
//______________________________________________________________________________

enum class mode { none, help, list, part };

struct settings {
  double      E_electron   = 18.0;  // GeV
  double      E_ion        = 275.0; // GeV
  std::string outfile      = "dis.hepmc";
  int         ion_PID      = 2212;
  int         electron_PID = 11;
  bool        help         = false;
  bool        success      = false;
  //double      Q2_min       = 4.0;
  double      Q2_min       = 100.0;
  int         N_events     = 100;
  mode        selected     = mode::none;
};

template <typename T>
void print_usage(T cli, const char* argv0)
{
  // used default formatting
  std::cout << "Usage:\n" << usage_lines(cli, argv0) << "\nOptions:\n" << documentation(cli) << '\n';
}
//______________________________________________________________________________

template <typename T>
void print_man_page(T cli, const char* argv0)
{
  // all formatting options (with their default values)
  auto fmt = clipp::doc_formatting{}
                 .start_column(8)                    // column where usage lines and documentation starts
                 .doc_column(20)                     // parameter docstring start col
                 .indent_size(4)                     // indent of documentation lines for children of a
                                                     // documented group
                 .line_spacing(0)                    // number of empty lines after single documentation lines
                 .paragraph_spacing(1)               // number of empty lines before and after paragraphs
                 .flag_separator(", ")               // between flags of the same parameter
                 .param_separator(" ")               // between parameters
                 .group_separator(" ")               // between groups (in usage)
                 .alternative_param_separator("|")   // between alternative flags
                 .alternative_group_separator(" | ") // between alternative groups
                 .surround_group("(", ")")           // surround groups with these
                 .surround_alternatives("(", ")")    // surround group of alternatives with these
                 .surround_alternative_flags("", "") // surround alternative flags with these
                 .surround_joinable("(", ")")        // surround group of joinable flags with these
                 .surround_optional("[", "]")        // surround optional parameters with these
                 .surround_repeat("", "...");        // surround repeatable parameters with these

  auto mp = make_man_page(cli, argv0, fmt);
  mp.prepend_section("DESCRIPTION", "Simple pythia dis generator.");
  mp.append_section("EXAMPLES", " $ pythia_dis [output_file]");
  std::cout << mp << "\n";
}
//______________________________________________________________________________

settings cmdline_settings(int argc, char* argv[])
{
  settings s;
  auto     lastOpt = " options:" % (option("-h", "--help").set(s.selected, mode::help) % "show help",
                                value("file", s.outfile).if_missing([] {
                                  std::cout << "You need to provide an output filename argument!\n";
                                }) % "output file");

  auto cli = (command("help").set(s.selected, mode::help) | lastOpt);

  assert(cli.flags_are_prefix_free());

  auto res = parse(argc, argv, cli);

  // if( res.any_error() ) {
  //  s.success = false;
  //  std::cout << make_man_page(cli, argv[0]).prepend_section("error: ",
  //                                                           "    The best
  //                                                           thing since
  //                                                           sliced bread.");
  //  return s;
  //}
  s.success = true;

  if (s.selected == mode::help) {
    print_man_page<decltype(cli)>(cli, argv[0]);
  };
  return s;
}
//______________________________________________________________________________

int main(int argc, char* argv[])
{

  settings s = cmdline_settings(argc, argv);
  if (!s.success) {
    return 1;
  }

  if (s.selected == mode::help) {
    return 0;
  }

  // Beam energies, minimal Q2, number of events to generate.
  double eProton   = s.E_ion;
  double eElectron = s.E_electron;
  double Q2min     = s.Q2_min;
  int    nEvent    = s.N_events;

  // Generator. Shorthand for event.
  Pythia pythia;
  Event& event = pythia.event;

  // Set up incoming beams, for frame with unequal beam energies.
  pythia.readString("Beams:frameType = 2");
  // BeamA = proton.
  pythia.readString("Beams:idA = " + std::to_string(s.ion_PID));
  pythia.settings.parm("Beams:eA", eProton);
  // BeamB = electron.
  pythia.readString("Beams:idB = 11");
  pythia.settings.parm("Beams:eB", eElectron);

  // Set up DIS process within some phase space.
  // Neutral current (with gamma/Z interference).
  pythia.readString("WeakBosonExchange:ff2ff(t:gmZ) = on");
  // Uncomment to allow charged current.
  // pythia.readString("WeakBosonExchange:ff2ff(t:W) = on");
  // Phase-space cut: minimal Q2 of process.
  pythia.settings.parm("PhaseSpace:Q2Min", Q2min);

  // Set dipole recoil on. Necessary for DIS + shower.
  pythia.readString("SpaceShower:dipoleRecoil = on");

  // Allow emissions up to the kinematical limit,
  // since rate known to match well to matrix elements everywhere.
  pythia.readString("SpaceShower:pTmaxMatch = 2");

  // QED radiation off lepton not handled yet by the new procedure.
  pythia.readString("PDF:lepton = off");
  pythia.readString("TimeShower:QEDshowerByL = off");

  // Initialize.
  pythia.init();

  // Interface for conversion from Pythia8::Event to HepMC one.
  HepMC3::Pythia8ToHepMC3 toHepMC;
  // Specify file where HepMC events will be stored.
  HepMC3::WriterAscii ascii_io(argv[1]);
  cout << endl << endl << endl;

  // Histograms.
  double Wmax = sqrt(4. * eProton * eElectron);
  Hist   Qhist("Q [GeV]", 100, 0., 50.);
  Hist   Whist("W [GeV]", 100, 0., Wmax);
  Hist   xhist("x", 100, 0., 1.);
  Hist   yhist("y", 100, 0., 1.);
  Hist   pTehist("pT of scattered electron [GeV]", 100, 0., 50.);
  Hist   pTrhist("pT of radiated parton [GeV]", 100, 0., 50.);
  Hist   pTdhist("ratio pT_parton/pT_electron", 100, 0., 5.);

  double sigmaTotal(0.), errorTotal(0.);
  bool   wroteRunInfo = false;
  // Get the inclusive x-section by summing over all process x-sections.
  double xs = 0.;
  for (int i = 0; i < pythia.info.nProcessesLHEF(); ++i) {
    xs += pythia.info.sigmaLHEF(i);
  }

  // Begin event loop.
  for (int iEvent = 0; iEvent < nEvent; ++iEvent) {
    if (!pythia.next())
      continue;

    double sigmaSample = 0., errorSample = 0.;

    // Four-momenta of proton, electron, virtual photon/Z^0/W^+-.
    Vec4 pProton = event[1].p();
    Vec4 peIn    = event[4].p();
    Vec4 peOut   = event[6].p();
    Vec4 pPhoton = peIn - peOut;

    // Q2, W2, Bjorken x, y.
    double Q2 = -pPhoton.m2Calc();
    double W2 = (pProton + pPhoton).m2Calc();
    double x  = Q2 / (2. * pProton * pPhoton);
    double y  = (pProton * pPhoton) / (pProton * peIn);

    // Fill kinematics histograms.
    Qhist.fill(sqrt(Q2));
    Whist.fill(sqrt(W2));
    xhist.fill(x);
    yhist.fill(y);
    pTehist.fill(event[6].pT());

    // pT spectrum of partons being radiated in shower.
    for (int i = 0; i < event.size(); ++i)
      if (event[i].statusAbs() == 43) {
        pTrhist.fill(event[i].pT());
        pTdhist.fill(event[i].pT() / event[6].pT());
      }

    // Get event weight(s).
    double evtweight = pythia.info.weight();

    // Do not print zero-weight events.
    if (evtweight == 0.)
      continue;

    // Create a GenRunInfo object with the necessary weight names and write
    // them to the HepMC3 file only once.
    if (!wroteRunInfo) {
      shared_ptr<HepMC3::GenRunInfo> genRunInfo;
      genRunInfo                  = make_shared<HepMC3::GenRunInfo>();
      vector<string> weight_names = pythia.info.weightNameVector();
      genRunInfo->set_weight_names(weight_names);
      ascii_io.set_run_info(genRunInfo);
      ascii_io.write_run_info();
      wroteRunInfo = true;
    }

    // Construct new empty HepMC event.
    HepMC3::GenEvent hepmcevt;

    // Work with weighted (LHA strategy=-4) events.
    double normhepmc = 1.;
    if (abs(pythia.info.lhaStrategy()) == 4)
      normhepmc = 1. / double(1e9 * nEvent);
    // Work with unweighted events.
    else
      normhepmc = xs / double(1e9 * nEvent);

    // Set event weight
    // hepmcevt.weights().push_back(evtweight*normhepmc);
    // Fill HepMC event
    toHepMC.fill_next_event(pythia, &hepmcevt);
    // Add the weight of the current event to the cross section.
    sigmaTotal += evtweight * normhepmc;
    sigmaSample += evtweight * normhepmc;
    errorTotal += pow2(evtweight * normhepmc);
    errorSample += pow2(evtweight * normhepmc);
    // Report cross section to hepmc
    shared_ptr<HepMC3::GenCrossSection> xsec;
    xsec = make_shared<HepMC3::GenCrossSection>();
    // First add object to event, then set cross section. This order ensures
    // that the lengths of the cross section and the weight vector agree.
    hepmcevt.set_cross_section(xsec);
    xsec->set_cross_section(sigmaTotal * 1e9, pythia.info.sigmaErr() * 1e9);
    // Write the HepMC event to file. Done with it.
    ascii_io.write_event(hepmcevt);

    // End of event loop. Statistics and histograms.
  }
  pythia.stat();
  cout << Qhist << Whist << xhist << yhist << pTehist << pTrhist << pTdhist;

  return 0;
}

