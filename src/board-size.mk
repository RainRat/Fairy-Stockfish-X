# One capacity selector; legacy Boolean options retain their defaults.
hugeboards ?= no
ifeq (,$(filter $(hugeboards),yes no))
  $(error hugeboards must be yes or no)
endif
boardbits ?= $(if $(filter yes,$(hugeboards)),512,$(if $(filter yes,$(verylargeboards)),256,$(if $(filter yes,$(largeboards)),128,64)))
boardbits := $(boardbits)
ifneq ($(words $(boardbits)),1)
  $(error boardbits must be 64, 128, 256, or 512)
endif
ifeq (,$(filter $(boardbits),64 128 256 512))
  $(error boardbits must be 64, 128, 256, or 512)
endif
ifneq ($(filter command line environment override,$(origin hugeboards)),)
  ifneq ($(hugeboards),$(if $(filter 512,$(boardbits)),yes,no))
    $(error hugeboards=$(hugeboards) conflicts with boardbits=$(boardbits))
  endif
endif
ifneq ($(filter command line environment override,$(origin largeboards)),)
  ifneq ($(largeboards),$(if $(filter 64,$(boardbits)),no,yes))
    $(error largeboards=$(largeboards) conflicts with boardbits=$(boardbits))
  endif
endif
ifneq ($(filter command line environment override,$(origin verylargeboards)),)
  ifneq ($(verylargeboards),$(if $(filter 256 512,$(boardbits)),yes,no))
    $(error verylargeboards=$(verylargeboards) conflicts with boardbits=$(boardbits))
  endif
endif
override largeboards = $(if $(filter 64,$(boardbits)),no,yes)
override verylargeboards = $(if $(filter 256 512,$(boardbits)),yes,no)
override hugeboards = $(if $(filter 512,$(boardbits)),yes,no)
override CXXFLAGS += -DBOARD_BITS=$(boardbits)
