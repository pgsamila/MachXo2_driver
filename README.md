# MachXo2_driver
QEMU emulation of the AXIOM Beta hardware / software

#How to transfer files?
Run `beta_file_transfer.sh` script on QEMU-AXIOM Beta when you need to transfer files from host to qemu. 
When QEMU-AXIOM Beta display `$Press Enter after files are coppied`, run the `host_file_transfer.sh` script from host PC.
After the `host_file_transfer.sh` finished running, press Enter on QEMU. Then your files will be transferd, compiled and converted to executeble in QEMU.

#NOTE
You will have to change the paths in the scripts acording to your PC and QEMU.
