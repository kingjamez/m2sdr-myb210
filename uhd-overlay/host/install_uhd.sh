
 

sudo rm -rf ./build 

mkdir build
cd build 
cmake ../
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../
sudo rm -rf ./build 
sudo uhd_find_devices 
sudo uhd_usrp_probe
