#!/usr/bin/env python3

import argparse
import subprocess
import os
import re

version_tag_regex = re.compile(r"v([0-9]+)\.([0-9]+)\.([0-9]+)")

def get_tags():
    return (s.strip().decode('utf-8') for s in subprocess.check_output(['git', 'tag'], stderr=subprocess.STDOUT).splitlines())

def parse_version(version):
    result = re.fullmatch(version_tag_regex, version)
    if result:
        return (int(result.group(1)), int(result.group(2)), int(result.group(3)))
    else:
        return None

def get_versions():
    for tag in get_tags():
        result = parse_version(tag)
        if result:
            yield result

def get_current_version():
    if os.path.isfile('VERSION'):
        with open('VERSION') as file:
            return parse_version(file.read().splitlines()[0].strip())
    return (1,0,0)

def get_next_version():
    current_version = get_current_version()
    bugfix_revs = [v[2] for v in get_versions() if v[0] == current_version[0] and v[1] == current_version[1]]
    
    if len(bugfix_revs) == 0:
        return (current_version[0], current_version[1], 0)
    else:
        next_bugfix_rev = max(bugfix_revs) + 1
        return (current_version[0], current_version[1], next_bugfix_rev)

def version_string(version):
    return f"v{version[0]}.{version[1]}.{version[2]}"

def write_version_file(version):
    with open('VERSION', 'w') as file:
        file.write(version_string(version))

def commit_version_file(version):
    subprocess.check_output(['git', 'add', 'VERSION'], stderr=subprocess.STDOUT)
    subprocess.check_output(['git', 'commit', '--allow-empty', '-m', 'ci: release of version ' + version_string(version)], stderr=subprocess.STDOUT)

def tag_version(version):
    subprocess.check_output(['git', 'tag', version_string(version)], stderr=subprocess.STDOUT)

def fetch_tags(username, password, organization, repository):
    fetch_url = f"https://{username}:{password}@github.com/{organization}/{repository}.git"
    subprocess.check_output(['git', 'fetch', '--tags', fetch_url], stderr=subprocess.STDOUT)

def push_tag(username, password, organization, repository, version):
    push_url = f"https://{username}:{password}@github.com/{organization}/{repository}.git"
    subprocess.check_output(['git', 'push', push_url], stderr=subprocess.STDOUT)
    subprocess.check_output(['git', 'push', push_url, version_string(version)], stderr=subprocess.STDOUT)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--username', '-u', help="The username to use when pushing tags.")
    parser.add_argument('--organization', '-o', help="The GitHub organization to push to.")
    parser.add_argument('--repository', '-r', help="The GitHub repository to push to.")
    parser.add_argument('--stage', '-s', choices=["pre-build", "post-build"], help="The stage to release stage to run.")
    args = parser.parse_args()

    password = os.getenv('AUTOMATION_USER_TOKEN')

    if args.stage == "pre-build":
        fetch_tags(args.username, password, args.organization, args.repository)
        next_version = get_next_version()
        write_version_file(next_version)
        print(f"version={version_string(next_version)}")
    elif args.stage == "post-build":
        fetch_tags(args.username, password, args.organization, args.repository)
        next_version = get_next_version()
        write_version_file(next_version)
        commit_version_file(next_version)
        tag_version(next_version)
        push_tag(args.username, password, args.organization, args.repository, next_version)
        print(f"version={version_string(next_version)}")
