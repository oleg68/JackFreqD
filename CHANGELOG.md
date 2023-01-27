# 0.2.4 (2023-01-27)
- Fixed unability to connect to the jack if -j was specified
# 0.2.3 (2022-09-01)
- Added terminate client threads when PipeWire server shuts down
- fixet setting of XDG_RUNTIME_DIR
# 0.2.2 (2021-05-22)
- Added autorestart on crash https://github.com/oleg68/JackFreqD/issues/1
- Fixed finding and connecting to pipewire daemons https://github.com/oleg68/JackFreqD/issues/2
- Eliminatted spamming of steps in the log
# 0.2.1 (2021-05-15)
- build on github
- shell scrips for build
- build rpm and deb
- build with cmake https://github.com/oleg68/jackfreqd/issues/3
- removed unsupported files
- enhanced logging

# 0.2.0 (2020-02-24)
- added support of the intel_pstate governor driver
- added a systemd service

# 0.1.3 (2011-05-24)
- changed init script to /bin/bash
- improved debug and warning message syntax.
- link with -lm

# 0.1.2 (2010-12-15)
- possible fix for jack-thread as root problem
- don't write log messsages while not root.

# 0.1.1 (2010-12-14)
- fix for Gabriel 'Voodoo' problem

# 0.1.0 (2010-12-12)
- added JACK process and UID detection based on procps
- change UID to access JACK
- hook into jack-graph change for triggering freq updates
- algorithm to choose CPU freq according to DSP and CPU load
- added man page & debian packagaing

# 0.0.1 (2010-12-12)
- first working version
- JACK DSP load polling

# 0.0.0 (2010-12-12)
- branch of powernowd-1.00
