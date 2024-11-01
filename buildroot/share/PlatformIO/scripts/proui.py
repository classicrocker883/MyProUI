# ------------------------------------------------------------------------------
# VSCode script for the Professional Firmware
# URL: https://github.com/mriscoc/Marlin_Ender3v2/releases
# Version: 3.2
# Date: 2024/07/17
# Author: Miguel Risco-Castillo
# ------------------------------------------------------------------------------

import shutil

libpath = "Marlin/lib/proui/"
libfile = libpath+'libproui.a'
from SCons.Script import DefaultEnvironment
env = DefaultEnvironment()
Import("env")

def _GetMarlinEnv(marlinEnv, feature):
   if not marlinEnv: return None
   return any(item.startswith(feature) for item in marlinEnv)

# Get Marlin evironment vars
if "MARLIN_FEATURES" in env:
   print("Processing Professional Firmware requirements")
   MarlinEnv = env['MARLIN_FEATURES']
   prouiex = _GetMarlinEnv(MarlinEnv, 'PROUI_EX')
   marlin_manualmesh = _GetMarlinEnv(MarlinEnv, 'MESH_BED_LEVELING')
   marlin_abl = _GetMarlinEnv(MarlinEnv, 'AUTO_BED_LEVELING_BILINEAR')
   marlin_ubl = _GetMarlinEnv(MarlinEnv, 'AUTO_BED_LEVELING_UBL')
   stm32f1 = _GetMarlinEnv(MarlinEnv, 'MCU_STM32F1')
   stm32f4 = _GetMarlinEnv(MarlinEnv, 'MCU_STM32F4')
   stm32g0 = _GetMarlinEnv(MarlinEnv, 'MCU_STM32G0')
   gd32f10 = _GetMarlinEnv(MarlinEnv, 'MAPLE_STM32F1')

   # Select correct architecture
   if stm32f1:
      arch = 'stm32f1/'
      print ('STM32F1 Architecture detected')
   elif stm32f4:
      arch = 'stm32f4/'
      print ('STM32F4 Architecture detected')
   elif stm32g0:
      arch = 'stm32g0/'
      print ('STM32G0 Architecture detected')
   elif (gd32f10):
      arch = 'gd32f10/'
      print ('GD32F1 Architecture detected')
   else:
      exit(NotImplementedError("Error: Can't detect the correct architecture"))

   # Select correct leveling system
   if marlin_manualmesh:
      lev = 'mbl'
      print("Manual Mesh Bed Leveling detected")
   elif marlin_abl:
      lev = 'abl'
      print("Auto Mesh Bed Leveling detected")
   elif marlin_ubl:
      lev = 'ubl'
      print("Unified Mesh Bed Leveling detected")
   else:
      lev = 'mbl'
      print("Default-No Probe detected")

   # Copy correct library file
   if prouiex:
      print('ProUI extension detected')
      libdestfile = libpath+arch+'libproui_'+lev+'.a'
      shutil.copy(libdestfile, libfile)
