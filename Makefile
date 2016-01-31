TOP = .
include $(TOP)/configure/CONFIG
DIRS := $(DIRS) $(filter-out $(DIRS), configure)
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *App))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocBoot))

define DIR_template
 $(1)_DEPEND_DIRS = configure
endef
$(foreach dir, $(filter-out configure,$(DIRS)),$(eval $(call DIR_template,$(dir))))

iocBoot_DEPEND_DIRS += $(filter %App,$(DIRS))

include $(TOP)/configure/RULES_TOP




## Quick star
#1. If you do not have the sources, git clone this reporsitory in your local folder ( referenced as the `<top>` folder).
#2. Ammend `<top>/configure/CONFIG_SITE` to match your site specifics.
#3. Move to `<top>` folder and run `make`
#4. Find the executable in `<top>/bin/<architecture>/caTools`

## executable
#bin/*

## latex source
#docs/*.log
#docs/*.aux
#docs/*.out
#docs/*.synctex.gz
#docs/*.toc
#docs/*.bbl
#docs/*.blg

## QT Creator project files
#*.config
#*.creator*
#*.files
#*.includes
