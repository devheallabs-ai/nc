"""
Convenience module for running NC workflows from Python.

Usage:
    from nc_lang import workflow

    result = workflow.run('''
        ask AI to "summarize" using doc
        respond with result
    ''', doc="your document text")

    print(result.output)
"""

from nc_lang.runtime import NC, NCResult

_default_nc = None


def _get_nc() -> NC:
    global _default_nc
    if _default_nc is None:
        _default_nc = NC()
    return _default_nc


def configure(**env_vars):
    """
    Configure the default NC runtime.

    Example:
        workflow.configure(
            NC_AI_KEY="sk-...",
            NC_AI_ADAPTER="anthropic"
        )
    """
    nc = _get_nc()
    for key, value in env_vars.items():
        nc.set_env(key, value)


def run(source: str, **variables) -> NCResult:
    """
    Run NC source code with context variables.

    Example:
        result = workflow.run('''
            ask AI to "translate to French" using text
            respond with result
        ''', text="Hello, world!")
    """
    return _get_nc().run(source, **variables)


def run_file(filename: str) -> NCResult:
    """Run an NC file."""
    return _get_nc().run_file(filename)
