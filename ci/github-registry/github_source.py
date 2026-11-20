"""Wait for an exact revision to be available from GitHub."""
import json
import os
import re
import time
import urllib.error
import urllib.request

def github_request(url):
    """FIX-GTS-295: api.github.com with the job's GITHUB_TOKEN when there is one; without it,
    calls share the runner's 60-an-hour anonymous limit (403 rate limit exceeded)."""
    token = os.environ.get('GITHUB_TOKEN', '')
    if not token or not url.startswith('https://api.github.com/'):
        return url
    return urllib.request.Request(url, headers={'Authorization': 'Bearer ' + token,
                                                'Accept': 'application/vnd.github+json'})


def wait_for_commit(api, commit, attempts=120):
    if not re.fullmatch(r'[0-9a-f]{40}', commit or ''):
        raise ValueError('Expected a full lowercase Git commit SHA')
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(github_request(api + '/commits/' + commit), timeout=30) as response:
                actual = json.load(response)['sha']
            if actual != commit:
                raise ValueError('GitHub returned a different commit')
            return commit
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise
            if attempt + 1 == attempts:
                raise RuntimeError('GitHub did not provide the requested revision') from None
            time.sleep(5)
