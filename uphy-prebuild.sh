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

# Generate device specific artifacts from U-Phy model
# Content in "generated" folder is overwritten 

if [[ $# -eq 0 ]] ; 
  then
    echo "U-Phy device generation disabled"
    echo "Device generation is enabled by passing the device model file to this script."
    echo "This is done by changing the prebuild configuration in the Makefile."
    echo "Example:"
    echo "PREBUILD=./uphy-prebuild.sh model/digio.json"
    echo ""
    echo "Create a new device model using https://devicebuilder.rt-labs.com/ "
    echo "and download the model file to the modus toolbox project."
    exit 0
fi

model=$1

os=$(uname -o)

echo "Running U-Phy Generator ($os)"

$(../mtb_shared/rtlabs-uphy-lib/latest-v1.X/bin/upgen.exe export -d generated --generator Code $model)
$(../mtb_shared/rtlabs-uphy-lib/latest-v1.X/bin/upgen.exe export -d generated --generator Profinet $model)
$(../mtb_shared/rtlabs-uphy-lib/latest-v1.X/bin/upgen.exe export -d generated --generator EtherNetIP $model)

echo "Generated files:"
ls generated/