# MCE aw91xxx LED plugin

"mce-plugin-aw91xxx" implements hybris-plugin compatible API for use
in devices that have aw91xxx based set of 5 indicator LEDs.

The idea of a "hybris-plugin" (this package) is shortly:

- it uses no mce functions or data types
- it can be compiled independently from mce

And the idea of "hybris-module" (part of mce) is:

- it contains functions with the same names as "hybris-plugin"
- if called, the functions will load & call "hybris-plugin" code
- if "hybris-plugin" is not present "hybris-module" functions still
  work, but return failures for everything

Put together:

- mce code can assume that hybris-plugin code is always available and
  callable during hw probing activity
- if hybris plugin is not installed / not compatible with underlying
  hw, failures will be reported and mce can try other existing ways to
  proble for available hw controls
