
# Testing JSON Parser (18-10-2026)

1. `git clone https://github.com/nst/JSONTestSuite.git`
2. Replace `programs` in `run_tests.py` with the following.
3. Compile `test_json-ws` by running `make test.json`.
3. Add `test_json-ws` under `parsers/test_wserver/`.

## Programs

```python
programs = {
    "C wserve":
        {
            "url": "",
            "commands": [os.path.join(PARSERS_DIR, "test_wserve/test_json-ws")]
        },
    "Awk JSON.awk gawk":
        {
            "url":"https://github.com/step-/JSON.awk",
            "commands":["/usr/bin/gawk", "-f", os.path.join(PARSERS_DIR, "test_JSON.awk", "JSON.awk")]
        },
    "Bash JSON.sh 2016-08-12":
        {
            "url":"https://github.com/dominictarr/JSON.sh",
            "commands":[os.path.join(PARSERS_DIR, "test_Bash_JSON/JSON.sh")],
            "use_stdin":True
        },
   "Free Pascal fcl-json":
       {
           "url":"",
           "commands":[os.path.join(PARSERS_DIR, "test_fpc/test_json")]
       },
   "JavaScript":
       {
           "url":"",
           "commands":["/usr/local/bin/node", os.path.join(PARSERS_DIR, "test_json.js")]
       },
   "Python3.12":
       {
           "url":"",
           "commands":["/usr/bin/env", "python3.12", os.path.join(PARSERS_DIR, "test_json.py")]
       },
   "Perl JSON::PP": # part of default install in perl >= v5.14
       {
           "url":"https://metacpan.org/pod/JSON::PP",
           "commands":["/usr/bin/perl", os.path.join(PARSERS_DIR, "test_json_pp.pl")]
       },
   "PHP 5.5.36":
       {
           "url":"",
           "commands":["/usr/bin/php", os.path.join(PARSERS_DIR, "test_json.php")]
       },
   "Nim":
         {
             "url":"http://nim-lang.org",
             "commands":[ os.path.join( PARSERS_DIR, "test_nim/test_json") ]
         },
}
```