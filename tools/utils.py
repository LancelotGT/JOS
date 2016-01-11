#!/usr/bin/env python2

import os
import subprocess
import socket
import string
import platform


KEY = os.path.expanduser("~/.cs3210.key")

#TODO
#distribution
URL = "https://tc.gtisc.gatech.edu/cs3210/2016/submit/handin.py"
#debugging
if "DEBUG" in os.environ:
    URL = "http://127.0.0.1:5000"

def curl(url, *args):
    print url
    try:
        html = subprocess.check_output(["curl"] + list(args) + [url],
                                       universal_newlines=True)
    except subprocess.CalledProcessError as e:
        html = e.output
    return html.strip()

def checkin():
    return curl("%s/checkin" % URL,
                "-s", "-f",
                "-F", "key=%s" % get_api_key())


def get_env():
    return "%s-%s" % (platform.architecture()[0][0:2], platform.release())


#
# api-key related
#
def check_api_key():
    # prompt for key if missing
    if not os.path.exists(KEY):
        print "Find your api-key at %s" % URL
        apikey = raw_input("Enter api-key> ").strip()
        with open(KEY, "w") as fd:
            fd.write(apikey + "\n")
        print("[!] written to %s" % KEY)

    if checkin() == "":
        print("[!] failed to connect to the submition site")
        print("[!] Please check the content of file cs3210.key matches the latest Api-Key you received through email")
        exit(1)

def get_api_key():
    return open(KEY).read().strip()
