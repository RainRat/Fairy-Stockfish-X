# Capacity from Boolean options; boardbits is internal only.
hugeboards ?= no
ifeq (,$(filter $(hugeboards),yes no))
  $(error hugeboards must be yes or no)
endif
ifneq ($(filter command line environment override,$(origin boardbits)),)
  $(error boardbits is removed; select capacity with largeboards=yes, verylargeboards=yes, or hugeboards=yes)
endif
boardbits := 64
ifeq ($(largeboards),yes)
  boardbits := 128
endif
ifeq ($(verylargeboards),yes)
  boardbits := 256
endif
ifeq ($(hugeboards),yes)
  boardbits := 512
endif
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
CXXFLAGS += -DBOARD_BITS=$(boardbits)
