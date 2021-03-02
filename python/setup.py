from setuptools import setup, find_packages

setup(
    name="plinth-client",
    version="0.1.0",
    author="Aneesh Krishna",
    description="Python client SDK for the Plinth stream-relational database",
    packages=find_packages(),
    python_requires=">=3.9",
    entry_points={
        "console_scripts": [
            "plinth=plinth.cli:main",
            "plinth-tui=plinth.tui:main",
        ],
    },
)
