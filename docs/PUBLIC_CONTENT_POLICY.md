# Public content and private evidence

Public docs, PR text, issue text and images must omit personal PC usernames, machine names, absolute filesystem/install paths, drive layout, local task/session identifiers, private network details, account/trial details and raw logs. Use repository-relative commands and explicit placeholders for local tool roots. Review screenshots for OS overlays, path text and other identifying content before uploading.

Keep full local receipts and logs in ignored evidence directories; retain them when sanitizing public summaries. Publish the source commit, test purpose, pass/fail result, counts, durations and material limits needed to assess a change. Public JSON evidence is a sanitized summary, not a byte-identical raw report; its local paths and identifiers may be placeholders. Do not invent a new test pass during redaction.

Inspect the staged diff and proposed publication text before each push. If a credential or other sensitive exposure is found, report its location privately and stop affected publication. Current-text redaction does not remove historical versions; history rewriting, credential rotation and deletion of local evidence require separate authorization.
