# W02_FreeModbus_v2b
I rewrited the FreeModbus lib in 2015, adding multi-master support 


## What Has Been Done?
1. Implemented multi-master support in RTU mode and successfully tested it with two masters. It supports both timed-polling and message event methods. When using the message event method, each slave requires an independent task thread, while timed-polling slaves can share a single task.

1. Completed multi-master support in ASCII mode and tested it with two slaves — one operating in RTU mode and the other in ASCII mode.

1. Developed multi-master support in TCP mode, which currently supports only TCP slaves. Additionally, it features software and hardware reconnection capabilities after connection loss.