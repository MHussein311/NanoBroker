from setuptools import setup, Extension
import sys
import pybind11
import os
import glob

# Define the C++ extension
ext_modules = [
    Extension(
        "nanobroker",
        ["src/python_bindings.cpp"],
        include_dirs=[
            "include",
            pybind11.get_include()
        ],
        language='c++',
        extra_compile_args=['-O3', '-std=c++17'],
        libraries=['rt', 'pthread']
    ),
]

setup(
    name="nanobroker",
    version="1.0.0",
    description="Zero-Copy Shared Memory Video Bridge for C++ and Python",
    ext_modules=ext_modules,
    py_modules=["nanobroker_config"],
    
    # --- NEW: This copies headers to /venv/include/nanobroker ---
    data_files=[
        ('include/nanobroker', glob.glob('include/nanobroker/*.hpp'))
    ],
)