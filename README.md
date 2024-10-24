# Proactive Congestion Notification Experiments

Replicate the results of article "Proactive Congestion Avoidance for Distributed Deep Learning"

Experiments replicated are:
1. Show that DDL environment produces congestion because of the bursty nature of the communication pattern with hugh data transfers.

## Dependencies
Project uses ns3 simulator to run the network simulation scripts. To install ns3, please verify if following dependencies are installed. If not installed, install using the commands given:

<b>
1. Git
</b>

```
sudo apt install git-all
```

<b>
2. g++ compiler
</b>

```
sudo apt install g++
```

<b>
3. Python3
</b>

```
sudo apt install python3
```

<b>
4. cmake
</b>

```
sudo apt install cmake
```

## ns3 install and build
Once all the dependencies are installed, you can install the ns3 simulator. Follow these instructions:
```
wget https://www.nsnam.org/releases/ns-allinone-3.43.tar.bz2
tar xfj ns-allinone-3.43.tar.bz2
cd ns-allinone-3.43/ns-3.43
```

Configure the ns3 simulator using following command and then build
```
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

Once the building is complete, you can run the test code to check your installtion and build:
```
./test.py
```