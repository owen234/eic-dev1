#!/bin/bash

print_env.sh

function print_the_help {
  echo "USAGE: ${0} -n <nevents> -t <nametag> -p <particle> "
  echo "  OPTIONS: "
  echo "    -n,--nevents     Number of events"
  echo "    -t,--nametag     name tag"
  echo "    -p,--particle    particle type"
  echo "    --pmin           minimum particle momentum (GeV)"
  echo "    --pmax           maximum particle momentum (GeV)"
  echo "                     allowed types: pion0, pion+, pion-, kaon0, kaon+, kaon-, proton, neutron, electron, positron, photon"
  exit
}

POSITIONAL=()
while [[ $# -gt 0 ]]
do
  key="$1"

  case $key in
    -h|--help)
      shift # past argument
      print_the_help
      ;;
    -t|--nametag)
      nametag="$2"
      shift # past argument
      shift # past value
      ;;
    -p|--particle)
      particle="$2"
      shift # past argument
      shift # past value
      ;;
    -n|--nevents)
      export JUGGLER_N_EVENTS="$2"
      shift # past argument
      shift # past value
      ;;
    --pmin)
      export FULL_CAL_PMIN="$2"
      shift
      shift
      ;;
    --pmax)
      export FULL_CAL_PMAX="$2"
      shift
      shift
      ;;
    --inputFile)
      export JUGGLER_FILE_NAME_TAG="$2"
      shift
      shift
      ;;
    *)    # unknown option
      #POSITIONAL+=("$1") # save it in an array for later
      echo "unknown option $1"
      print_the_help
      shift # past argument
      ;;
  esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [[ ! -n  "${JUGGLER_N_EVENTS}" ]] ; then
  export JUGGLER_N_EVENTS=1000
fi

if [[ ! -n  "${FULL_CAL_PMIN}" ]] ; then
  export FULL_CAL_PMIN=1.0
fi

if [[ ! -n  "${FULL_CAL_PMAX}" ]] ; then
  export FULL_CAL_PMIN=10.0
fi

if [[ ! -n "${JUGGLER_DETECTOR_PATH}" ]] ; then
  export JUGGLER_DETECTOR_PATH=${DETECTOR_PATH}
fi

compact_path=${JUGGLER_DETECTOR_PATH}/${JUGGLER_DETECTOR}.xml
#export JUGGLER_FILE_NAME_TAG="${nametag}"
export JUGGLER_GEN_FILE="${JUGGLER_FILE_NAME_TAG}.hepmc"

export JUGGLER_SIM_FILE="sim_${JUGGLER_FILE_NAME_TAG}.root"
export JUGGLER_DIGI_FILE="digi_${JUGGLER_FILE_NAME_TAG}.root"
export JUGGLER_REC_FILE="rec_${JUGGLER_FILE_NAME_TAG}.root"

echo "JUGGLER_N_EVENTS = ${JUGGLER_N_EVENTS}"
echo "JUGGLER_DETECTOR_PATH = ${JUGGLER_DETECTOR_PATH}"
echo "JUGGLER_DETECTOR = ${JUGGLER_DETECTOR}"

# Generate the input events
#python benchmarks/imaging_ecal/scripts/gen_particles.py ${JUGGLER_GEN_FILE} -n ${JUGGLER_N_EVENTS}\
#    --angmin 5 --angmax 175 --phmin 0 --phmax 360 --pmin ${FULL_CAL_PMIN} --pmax ${FULL_CAL_PMAX}\
#    --particles="${particle}"

if [[ "$?" -ne "0" ]] ; then
  echo "ERROR running script: generating input events"
  exit 1
fi

ls -lh ${JUGGLER_GEN_FILE}

# Run geant4 simulations
npsim --runType batch \
      -v WARNING \
      --part.minimalKineticEnergy "1*TeV" \
      --numberOfEvents ${JUGGLER_N_EVENTS} \
      --compactFile ${compact_path} \
      --inputFiles ${JUGGLER_GEN_FILE} \
      --outputFile ${JUGGLER_SIM_FILE}

if [[ "$?" -ne "0" ]] ; then
  echo "ERROR running npdet"
  exit 1
fi

rootls -t "${JUGGLER_SIM_FILE}"

# Directory for results
mkdir -p results

##########FULL_CAL_OPTION_DIR=/home/vagrant/development/reconstruction_benchmarks/benchmarks/clustering/options
##########FULL_CAL_SCRIPT_DIR=/home/vagrant/development/reconstruction_benchmarks/benchmarks/clustering/scripts

# Run Juggler
# Digitization
#xenv -x ${JUGGLER_INSTALL_PREFIX}/Juggler.xenv \
#    gaudirun.py ${FULL_CAL_OPTION_DIR}/depricated/full_cal_digi.py
#if [[ "$?" -ne "0" ]] ; then
#  echo "ERROR running digitization (juggler)"
#  exit 1
#fi

## Clustering
#xenv -x ${JUGGLER_INSTALL_PREFIX}/Juggler.xenv \
#    gaudirun.py ${FULL_CAL_OPTION_DIR}/depricated/full_cal_clusters.py
#if [[ "$?" -ne "0" ]] ; then
#  echo "ERROR running clustering (juggler)"
#  exit 1
#fi

xenv -x ${JUGGLER_INSTALL_PREFIX}/Juggler.xenv \
    gaudirun.py ${FULL_CAL_OPTION_DIR}/full_cal_reco.py
if [[ "$?" -ne "0" ]] ; then
  echo "ERROR running CAL reco (juggler)"
  exit 1
fi



# Run analysis scripts
python ${FULL_CAL_SCRIPT_DIR}/cluster_plots.py ${JUGGLER_SIM_FILE} ${JUGGLER_REC_FILE} -o results

root_filesize=$(stat --format=%s "${JUGGLER_REC_FILE}")
if [[ "${JUGGLER_N_EVENTS}" -lt "500" ]] ; then
  # file must be less than 10 MB to upload
  if [[ "${root_filesize}" -lt "10000000" ]] ; then
    cp ${JUGGLER_REC_FILE} results/.
  fi
fi

