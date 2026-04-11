from setuptools import setup, find_packages

setup(
    name="nc-lang",
    version="1.0.0",
    description="NC — The AI Language. Write AI APIs in plain English.",
    long_description=open("README.md").read(),
    long_description_content_type="text/markdown",
    author="DevHeal Labs AI",
    author_email="support@devheallabs.in",
    url="https://github.com/devheallabs-ai/nc-lang",
    project_urls={
        "Documentation": "https://github.com/devheallabs-ai/nc-lang/blob/main/docs/NC_USER_MANUAL.md",
        "Source": "https://github.com/devheallabs-ai/nc-lang",
        "Issues": "https://github.com/devheallabs-ai/nc-lang/issues",
    },
    packages=find_packages(),
    python_requires=">=3.8",
    entry_points={
        "console_scripts": [
            "nc=nc_lang.cli:main",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 3",
    ],
    keywords="nc ai workflow llm openai anthropic gemini plain-english orchestration",
    license="Apache-2.0",
)

