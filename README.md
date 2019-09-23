<img src="http://pencode.net/default.png" alt="logo" style="zoom:12%;" /> 

## Shoit

Shoit is **Super High Output ** . A toolkit for High Performance Data transmission

Shoit is a  toolkit  for High Performance Data transmission. It is protocol specifically 

designed to move very large datas.

Shoit  goal is move very large datas over  high-speed networks - on the order of gigabits per second. 

Shoit include receiver and sender.  

[ shoit sender] ---- [ *wide area high-speed networks* ] --- [shoit receiver]



### Principle

How to high-speed  move data?

The first is to keep the network pipe as full as possible during bulk data transfer. 

The second goal is to avoid TCP’s per-packet interaction so that acknowledgments are not sent per window 

of transmitted data, but aggregated and delivered at the end of a transmission phase.



## Features

- Runs on UDP.  
- Supports big file and data stream transfer
- Works on Linux.



## Building

git clone https://github.com/pencode-lab/Shoit.git

$cd Shoit 

$make





## Example usage on the sender

- ##### send file 

./sendfile 12g.data ticket -w 1000000

-w 1000000  specifies the bind width is 1000000 Kbits

more options run ./sendfile -h

- ##### send stream

 tar cf - <send file>| ./sendstream [password] [options]



## Example usage on the recv

- ##### recv file

  ./recvfile [sender host] [password]

- ##### recv stream

  ./recvstream  [sender host] [password] | tar xvpf -



## Advanced configuration

Sender Options:

  -w <Kbps>         specifies the bind width(default 100Mbits: 100000)

  -l <Bytes>        specifies the packet size (default: MTU)

  -s <bind server>  specifies the bind ip or host (default:0.0.0.0)

  -p <number>       specifies the listen port number (default: 38000)

  -v                Show version and copyight infomation

  -d                Run as daemon

  -E <log_file>     print log infomation to file,(default:print to stderr)

  -h                prints this help



Receiver Options:

  -f <fileName> Specifies the save file name(default: xxxx.shoit.recv.time)

  -p <number>   Specifies the remote(sender) port number(default: 38000)

  -v            Show version and copyight infomation

  -E <log_file> print log infomation to file,(default:print to stderr)

  -d            Run as daemon

  -h            prints this help





## Related projects

- [Gunnel](http://pencode.net/Gunnel.html) is a QUIC-based tunnel that support multiple clients.

  
  
  

## source code

open source on the GitHub [Shoit](https://github.com/pencode-lab/Shoit)



