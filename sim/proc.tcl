proc build-tcp { type src dest pktSize window class startTime } {
   global ns

   if { $type == "TCP" } {
      set tcp [new Agent/TCP]
      set snk [new Agent/TCPSink]
   } elseif { $type == "Reno" } {
      set tcp [new Agent/TCP/Reno]
      set snk [new Agent/TCPSink]
   } elseif { $type == "Sack" } {
      set tcp [new Agent/TCP/Sack1]
      set snk [new Agent/TCPSink/Sack1]
   } elseif  { $type == "Newreno" } {
      set tcp [new Agent/TCP/Newreno]
      set snk [new Agent/TCPSink]
   } else {
      puts "ERROR: Inavlid tcp type"
   }
   $ns attach-agent $src $tcp

   #$tcp set tcpTick_ 0.01

   $ns attach-agent $dest $snk

   $ns connect $tcp $snk

   if { $pktSize > 0 } {
      $tcp set packetSize_ $pktSize
   }
   $tcp set class_ $class
   if { $window > 0 } {
      $tcp set window_ $window
   } else {
      # default in ns-2 version 2.0
      $tcp set window_ 20
   }

   set ftp [new Source/FTP]
   $ftp set agent_ $tcp
   $ns at $startTime "$ftp start"

   return $tcp
}


proc build-udp { src dest pktSize interval random id startTime } {
   global ns

   set udp [new Agent/CBR]
   $ns attach-agent $src $udp

   set null [new Agent/Null]
   $ns attach-agent $dest $null

   $ns connect $udp $null

   if {$pktSize > 0} {  
        $udp set packetSize_ $pktSize
   }

   $udp set fid_ $id
   $udp set interval_ $interval
   $udp set random_ $random
   $ns at $startTime "$udp start"

   return $udp
}

proc build-on-off { src dest pktSize burstTime idleTime rate id startTime } {
   global ns

   set cbr [new Agent/CBR/UDP]
   $ns attach-agent $src $cbr

   set null [new Agent/Null]
   $ns attach-agent $dest $null

   $ns connect $cbr $null

   set exp1 [new Traffic/Expoo]
   $exp1 set packet-size $pktSize
   $exp1 set burst-time $burstTime
   $exp1 set idle-time $idleTime
   $exp1 set rate $rate
   $cbr  attach-traffic $exp1

   $ns at $startTime "$cbr start"
   $cbr set fid_ $id
   return $cbr
}

proc build-udt { src dest mtu window id startTime } {
   global ns

   set udt0 [new Agent/UDT]
   set udt1 [new Agent/UDT]

   $udt0 set mtu_ $mtu
   $udt0 set max_flow_window_ $window

   $ns attach-agent $src $udt0
   $ns attach-agent $dest $udt1
   $ns connect $udt0 $udt1

   set ftp [new Application/FTP]
   $ftp attach-agent $udt0

   $udt0 set fid_ $id

   $ns at $startTime "$ftp start"

   return $udt0
}
