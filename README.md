# eic-dev1

Recipe tested on 2021-07-07


1.  Set up your virtual box working area
```
       cd <your-vm-singularity-directory>
       
       vagrant ssh
```

2.  If you don't already have this set up or want to redo it, do this.
    You can choose your own directory name.  Here, it's "my-work1"
```
       mkdir my-work1    
       cd my-work1       
       curl https://eicweb.phy.anl.gov/containers/eic_container/-/raw/master/install.sh | bash
```

   This will take a while as it will download about 2.3 GB of stuff.
    
    
3.  Start the eic shell from your directory (here, it's my-work1)
```
       ./eic-shell
```

4.  Make a development directory, check out some stuff, compile, ...
```
       mkdir development
       export LD_LIBRARY_PATH=$PWD/development/lib:$LD_LIBRARY_PATH
       export PATH=$PWD/development/bin:$PATH

       git clone https://eicweb.phy.anl.gov/EIC/detectors/athena.git
       cd athena
       mkdir build && cd build
       cmake .. -DCMAKE_INSTALL_PREFIX=../../development |& tee cmake.log
       make install |& tee build.log
       cd ../..

       git clone https://eicweb.phy.anl.gov/EIC/detectors/ip6.git
       cd ip6
       mkdir build && cd build
       cmake .. -DCMAKE_INSTALL_PREFIX=../../development |& tee cmake.log
       make install |& tee build.log
       cd ../..
       cp -r ip6/ip6 athena/

       git clone https://eicweb.phy.anl.gov/EIC/juggler.git
       cd juggler/
       mkdir build && cd build
       cmake .. -DCMAKE_INSTALL_PREFIX=../../development -DCMAKE_CXX_STANDARD=20 |& tee cmake.log
       make install |& tee build.log
       cd ../..

       export DETECTOR_PATH=$PWD/athena
       export JUGGLER_DETECTOR=athena
       export JUGGLER_INSTALL_PREFIX=$PWD/development

       git clone https://eicweb.phy.anl.gov/EIC/benchmarks/common_bench.git
       export LOCAL_PREFIX=$PWD/common_bench
       export PATH=$PWD/common_bench/bin:$PATH

       git clone https://eicweb.phy.anl.gov/EIC/benchmarks/reconstruction_benchmarks.git
       export FULL_CAL_OPTION_DIR=$PWD/reconstruction_benchmarks/benchmarks/clustering/options
       export FULL_CAL_SCRIPT_DIR=$PWD/reconstruction_benchmarks/benchmarks/clustering/scripts
```

5. Check out my working code
```
       git clone https://github.com/owen234/eic-dev1.git
       cd eic-dev1/
```


6. Build an executable that runs pythia (the MC generator) and run it
```
       sh build-pythia-exe.sh

       ./pythia_dis gen-100evts.hepmc
```
   Note that there are a few settings at the top of the source code file (pythia_dis.cxx).
   For example, N_events and Q2_min.  You will probably need to adjust these.  When you do,
   recompile with build-pythia-exe.sh.


7. Run the detector simulation and reconstruction

     First, edit the file athena/compact/ecal.xml.  For now, make sure that ecal_barrel.xml is used,
     not ecal_barrel_hybrid.xml.  That is, it should look something like this in athena/compact/ecal.xml

```
          <include ref="ecal_barrel.xml"/>
          <!--include ref="ecal_barrel_hybrid.xml"/-->
```

  In the above two lines, the second one is a comment (note the extra !-- at the beginning and -- at the end).

  Now, run the detector simulation and reconstruction.

```
       bash full_cal_clusters.sh --inputFile gen-100evts --nevents 100 |& tee reco-100evts-try1a.log
```

