package=binutils-avr
$(package)_version=2.26
$(package)_download_path=https://launchpad.net/ubuntu/+archive/primary/+sourcefiles/binutils-avr/
$(package)_file_name=2.26.20160125+Atmel3.6.0-1/binutils-avr_2.26.20160125+Atmel3.6.0.orig.tar.gz
$(package)_sha256_hash=9dab89197ef85fc68d8a5e0d51559337bf06f86fa5e09d098acddedf668ba49e

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
