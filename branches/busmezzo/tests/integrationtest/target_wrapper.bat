@echo off
SetLocal EnableDelayedExpansion
(set PATH=C:\QT\QT5.8.0\5.8\MSVC2015_64\bin;!PATH!)
if defined QT_PLUGIN_PATH (
    set QT_PLUGIN_PATH=C:\QT\QT5.8.0\5.8\MSVC2015_64\plugins;!QT_PLUGIN_PATH!
) else (
    set QT_PLUGIN_PATH=C:\QT\QT5.8.0\5.8\MSVC2015_64\plugins
)
%*
EndLocal
