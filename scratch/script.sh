d=8
r=1
  maxAmpduSize=65536
      duration=5
    enablePcap=true
    mcs=8
    n=10
    nBss=1
    uplink=200

rm ../*.pcap
  
../waf --run "wifi-spatial-reuse --uplinkA=${uplink} --uplinkB=${uplink} --d=$d --r=$r --mcs=$mcs -maxAmpduSize=$maxAmpduSize --enablePcap=$enablePcap --duration=$duration --n=$n --nBss=${nBss}" 
