# Fix Touchscreen Standy Problem

## Issue

After sleep the touchscreen does not work anymore.

## How to fix

In many (not all) cases, it helps to unload the goodix kernel driver before sleep and restart it after resume. The script provided in this folder does that.