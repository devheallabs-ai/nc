"""
NC Language — Python Bindings

Embed NC (The AI Workflow Language) in your Python applications.

Usage:
    from nc_lang import NC

    nc = NC()
    nc.set_env("NC_AI_KEY", "sk-...")

    result = nc.run('''
        ask AI to "summarize this document" using doc:
            save as: summary
        respond with summary
    ''', doc="Your document text here")

    print(result.output)

Or use the convenience function:
    from nc_lang import workflow

    result = workflow.run('''
        ask AI to "classify" using text
        respond with result
    ''', text="some input")
"""

from nc_lang.runtime import NC, NCResult
from nc_lang import workflow

__version__ = "1.0.0"
__all__ = ["NC", "NCResult", "workflow"]
