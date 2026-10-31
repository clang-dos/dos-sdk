#!/bin/sh
# Run once as root inside the Mac OS X 10.4 guest, from the folder that
# holds this script, to install the test runner:  sudo sh install.sh
set -e
mkdir -p /usr/local/bin
cp clangdos-testrunner /usr/local/bin/
chmod 755 /usr/local/bin/clangdos-testrunner
cp org.clang-dos.testrunner.plist /Library/LaunchDaemons/
chown root:wheel /Library/LaunchDaemons/org.clang-dos.testrunner.plist
chmod 644 /Library/LaunchDaemons/org.clang-dos.testrunner.plist
echo "installed; shut down and keep this disk image as the test image"
