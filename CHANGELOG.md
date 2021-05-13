# 0.2.1 (2021-05-11)
- shell scrips for build
- build with cmake
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