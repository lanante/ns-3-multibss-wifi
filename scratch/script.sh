d1=1
d2=1
d3=10
  maxAmpduSize=3000
      duration=50
    enablePcap=true
    mcs=0
../waf --run "wifi-spatial-reuse --d1=$d1 --d2=$d2 --d3=$d3 --mcs=$mcs -maxAmpduSize=$maxAmpduSize --enablePcap=$enablePcap --duration=$duration"
