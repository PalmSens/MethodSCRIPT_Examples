# How to build the MethodSCRIPT example PDFs?

Dependencies:
- python
- asciidoctor-pdf
- rouge (code highlighting)

When you have made updates to the documentation you can build test PDFs by running 'make.py'.

The PDF drafts will be put in an 'output' folder in the 'Documentation' directory.

You can check the PDFs and see if there are any mistakes.

If you are sure everything is okay you can run 'make.py --final'. This will put the PDFs in their respective example folders and overwrite the existing documentation.