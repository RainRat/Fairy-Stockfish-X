# Capacity from Boolean options; boardbits is internal only.
hugeboards ?= no
ifeq (,$(filter $(hugeboards),yes no))
  $(error hugeboards must be yes or no)
endif
ifneq ($(filter command line environment override,$(origin boardbits)),)
  $(error boardbits is removed; select capacity with hugeboards=yes, verylargeboards=yes, or largeboards=yes)
endif
boardbits := $(if $(filter yes,$(hugeboards)),512,$(if $(filter yes,$(verylargeboards)),256,$(if $(filter yes,$(largeboards)),128,64)))
ifneq ($(filter command line environment override,$(origin largeboards)),)
  ifneq ($(largeboards),$(if $(filter 64,$(boardbits)),no,yes))
    $(error largeboards=no conflicts with verylargeboards=yes or hugeboards=yes)
  endif
endif
ifneq ($(filter command line environment override,$(origin verylargeboards)),)
  ifneq ($(verylargeboards),$(if $(filter 256 512,$(boardbits)),yes,no))
    $(error verylargeboards=no conflicts with hugeboards=yes)
  endif
endif
largeboards = $(if $(filter 64,$(boardbits)),no,yes)
verylargeboards = $(if $(filter 256 512,$(boardbits)),yes,no)
hugeboards = $(if $(filter 512,$(boardbits)),yes,no)
# Capacity must survive user-supplied CXXFLAGS (which replace makefile flags
# upstream); other flags follow upstream replacement behavior.
override CXXFLAGS += -DBOARD_BITS=$(boardbits)
