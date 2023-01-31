#!/bin/bash

 # Generate the initial conditions if they are not present.
if [ ! -e EAGLE_ICs_6.hdf5 ]
then
    echo "Fetching initial conditions for the EAGLE 6Mpc example..."
    ./getIC.sh
fi

../../../swift -PInitialConditions:replicate:2 -v 1 --interleave --hydro --threads=4 -n 16 -y 1 eagle_6.yml | tee output.log

