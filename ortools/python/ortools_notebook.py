"""Launch the notebook server."""

import re
import sys

from notebook import notebookapp


if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw?|\.exe)?$', '', sys.argv[0])
    sys.exit(notebookapp.main())
