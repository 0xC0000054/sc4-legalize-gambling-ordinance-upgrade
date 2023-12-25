# sc4-legalize-gambling-ordinance-upgrade

A DLL Plugin for SimCity 4 that updates the built-in Legalized Gambling ordinance to have its income based on the city's residential population.

**Availability Requirements:** None.    
**Income:** Base income of §250/month, plus additional income based on the residential wealth group populations.    
**Effects:**

Crime Effect: +20%.

This ordinance replaces the game's built-in Legalized Gambling ordinance, it is incompatible with any mod that alters the built-in ordinance.
This mod ignores the ordinance Exemplar and only uses the LTEXT files from the built-in ordinance.

The plugin can be downloaded from the Releases tab: https://github.com/0xC0000054/sc4-legalize-gambling-ordinance-upgrade/releases

## System Requirements

* Windows 10 or later

The plugin may work on Windows 7 or later with the [Microsoft Visual C++ 2022 x86 Redistribute](https://aka.ms/vs/17/release/vc_redist.x86.exe) installed, but I do not have the ability to test that.

## Installation

1. Close SimCity 4.
2. Copy `SC4LegalizeGamblingUpgrade.dll` and `SC4LegalizeGamblingUpgrade.ini` into the Plugins folder in the SimCity 4 installation directory.
3. Configure the plugin settings, see the `Configuring the plugin` section.

## Configuring the plugin

1. Open `SC4LegalizeGamblingUpgrade.ini` in a text editor (e.g. Notepad).    
Note that depending on the permissions of your SimCity 4 installation directory you may need to start the text editor 
with administrator permissions to be able to save the file.

2. Adjust the settings in the `[GamblingOrdinance]` section to your preferences.

3. Save the file and start the game.

### Settings overview:  

`BaseMonthlyIncome` is the base monthly income provided by the ordinance, defaults to §250.

#### Wealth Group Income Factors

The following values represent the factors (multipliers) that control how
much each wealth group contributes to the monthly income based on the group's population.
A value of 0.0 excludes the specified wealth group from contributing to the monthly income.
For example, if the city's R§ population is 1000 and the R$ income factor is 0.05
the R$ group would contribute an additional §50 to the monthly income total.

`R$IncomeFactor` income factor for the R§ population, defaults to 0.02.
`R$$IncomeFactor` income factor for the R§§ population, defaults to 0.03.
`R$$$IncomeFactor` income factor for the R§§§ population, defaults to 0.05.

#### Ordinance Effects

The following options control the effects that the ordinance has.

`CrimeEffectMultiplier` the effect that the ordinance has on global city crime. Defaults to 1.20, a +20% increase.
The value uses a range of [0.01, 2.0] inclusive, a value of 1.0 has no effect. Values below 1.0 reduce crime, and values above 1.0 increase crime.

## Troubleshooting

The plugin should write a `SC4LegalizeGamblingUpgrade.log` file in the same folder as the plugin.    
The log contains status information for the most recent run of the plugin.

# License

This project is licensed under the terms of the MIT License.    
See [LICENSE.txt](LICENSE.txt) for more information.

## 3rd party code

[gzcom-dll](https://github.com/nsgomez/gzcom-dll/tree/master) Located in the vendor folder, MIT License.    
[Windows Implementation Library](https://github.com/microsoft/wil) MIT License    
[Boost.PropertyTree](https://www.boost.org/doc/libs/1_83_0/doc/html/property_tree.html) Boost Software License, Version 1.0.

# Source Code

## Prerequisites

* Visual Studio 2022

## Building the plugin

* Open the solution in the `src` folder
* Update the post build events to copy the build output to you SimCity 4 application plugins folder.
* Build the solution

## Debugging the plugin

Visual Studio can be configured to launch SimCity 4 on the Debugging page of the project properties.
I configured the debugger to launch the game in a window with the following command line:    
`-intro:off -CPUcount:1 -w -CustomResolution:enabled -r1920x1080x32`

You may need to adjust the resolution for your screen.
