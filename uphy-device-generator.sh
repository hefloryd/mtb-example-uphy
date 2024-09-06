# ********************************************************************
#        _       _         _
#  _ __ | |_  _ | |  __ _ | |__   ___
# | '__|| __|(_)| | / _` || '_ \ / __|
# | |   | |_  _ | || (_| || |_) |\__ \
# |_|    \__|(_)|_| \__,_||_.__/ |___/
#
# www.rt-labs.com
# Copyright 2024 rt-labs AB, Sweden.
# See LICENSE file in the project root for full license information.
# *******************************************************************/
#
# Generate device specific artifacts from a U-Phy model.
#
# The model model json file is passed as argument
# Generated artifacts are stored in the "generated" folder.
# The content in the "generated" folder is overwritten.
#
# Note that the path to rtlabs-uphy-lib including version in the 
# modus workspace is used to locate the upgen executable.
#

if [[ $# -eq 0 ]] ; 
  then
    echo "Error - pass the device model as argument"
    echo "Example:"
    echo "  ./uphy-device-generator.sh model/digio.json"
    echo ""
    echo "Create a new device model using https://devicebuilder.rt-labs.com/ "
    echo "and download the model file to the modus toolbox project."
    exit 0
fi

model=$1
destination=generated
uphy_lib_version=latest-v1.x

os=$(uname -o)
if [[ "$os" == "Linux" ]]; then
  tool="../mtb_shared/rtlabs-uphy-lib/$uphy_lib_version/bin/upgen"
elif [[ "$os" == "Cygwin" ]]; then
  tool="../mtb_shared/rtlabs-uphy-lib/$uphy_lib_version/bin/upgen.exe"
else
  echo "Error - upgen not available for $os"
  exit -1
fi

if ! test -f "$tool"; then
  echo "Error - did not find upgen executable ($tool)"
  echo "Update uphy_lib_version or path in this script."
  exit -1
fi

echo "Run U-Phy Generator"
set -x
$($tool export -d generated --generator Code $model)
$($tool export -d generated --generator Profinet $model)
$($tool export -d generated --generator EtherNetIP $model)

