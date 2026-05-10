#!/bin/bash

echo "========================================="
echo "Bukana Health System - Git Update Script"
echo "========================================="

# Check if we're in the right directory
if [ ! -f "main.cpp" ]; then
    echo "Error: main.cpp not found. Run this from the project root."
    exit 1
fi

# Ensure directories exist
mkdir -p screenshots docs

# Generate documentation if markdown exists
if [ -f "docs/system-documentation.md" ]; then
    echo "Generating PDF documentation..."
    cd docs
    pandoc system-documentation.md -o system-documentation.pdf \
        --pdf-engine=pdflatex \
        --toc --toc-depth=3 --number-sections \
        --highlight-style=tango \
        -V colorlinks=true -V linkcolor=blue -V urlcolor=cyan
    cd ..
    echo "✓ Documentation PDF generated"
fi

# Add all files
echo "Adding files to git..."
git add .

# Show status
echo "Current status:"
git status

# Commit
echo ""
echo "Enter commit message (or press Enter for default):"
read commit_msg

if [ -z "$commit_msg" ]; then
    commit_msg="Update Bukana Health System - $(date '+%Y-%m-%d %H:%M:%S')"
fi

git commit -m "$commit_msg"

# Push
echo ""
echo "Pushing to GitHub..."
git push origin main

echo ""
echo "✓ Update complete!"
echo "Repository: https://github.com/tahleho3968/Bukana-Child-Vaccination-Record-Management-System"
