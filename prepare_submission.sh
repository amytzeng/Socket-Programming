#!/bin/bash

# Script to prepare Phase 1 submission package
# Usage: ./prepare_submission.sh <student_id>

if [ -z "$1" ]; then
    echo "Usage: $0 <student_id>"
    echo "Example: $0 b02705001"
    exit 1
fi

STUDENT_ID=$1
SUBMISSION_NAME="${STUDENT_ID}_part1"
TEMP_DIR="${SUBMISSION_NAME}"

echo "========================================="
echo "  Preparing Phase 1 Submission Package  "
echo "========================================="
echo "Student ID: $STUDENT_ID"
echo "Package name: ${SUBMISSION_NAME}.tar.gz"
echo ""

# Clean previous builds
echo "[1/7] Cleaning previous builds..."
make clean > /dev/null 2>&1

# Build the client
echo "[2/7] Compiling client..."
make > /dev/null 2>&1

if [ ! -f "client" ]; then
    echo "ERROR: Client compilation failed!"
    echo "Please fix compilation errors first."
    exit 1
fi

echo "       ✓ Client compiled successfully"

# Check if PDF documentation exists
if [ ! -f "README.pdf" ] && [ ! -f "documentation.pdf" ] && [ ! -f "manual.pdf" ]; then
    echo "[3/7] WARNING: PDF documentation not found!"
    echo "       Please convert README.md to PDF before submission."
    echo "       Expected filename: README.pdf or documentation.pdf"
    echo ""
    echo "       You can convert using:"
    echo "       - macOS: Open README.md in Preview, then Export as PDF"
    echo "       - Online: https://www.markdowntopdf.com/"
    echo "       - Command line: pandoc README.md -o README.pdf"
    echo ""
    read -p "       Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo "[3/7] ✓ PDF documentation found"
fi

# Create temporary directory
echo "[4/7] Creating package directory..."
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR"

# Copy required files
echo "[5/7] Copying files..."

# 1. Source code
cp client.cpp "$TEMP_DIR/"
echo "       ✓ Source code (client.cpp)"

# 2. Binary executable
cp client "$TEMP_DIR/"
echo "       ✓ Binary executable (client)"

# 3. Makefile
cp Makefile "$TEMP_DIR/"
echo "       ✓ Makefile"

# 4. PDF documentation (try different names)
if [ -f "README.pdf" ]; then
    cp README.pdf "$TEMP_DIR/"
    echo "       ✓ Documentation (README.pdf)"
elif [ -f "documentation.pdf" ]; then
    cp documentation.pdf "$TEMP_DIR/"
    echo "       ✓ Documentation (documentation.pdf)"
elif [ -f "manual.pdf" ]; then
    cp manual.pdf "$TEMP_DIR/"
    echo "       ✓ Documentation (manual.pdf)"
else
    echo "       ⚠ No PDF documentation included"
fi

# Create tarball
echo "[6/7] Creating tarball..."
tar -czf "${SUBMISSION_NAME}.tar.gz" "$TEMP_DIR"

if [ -f "${SUBMISSION_NAME}.tar.gz" ]; then
    echo "       ✓ Tarball created successfully"
else
    echo "ERROR: Failed to create tarball"
    exit 1
fi

# Verify tarball
echo "[7/7] Verifying package..."
tar -tzf "${SUBMISSION_NAME}.tar.gz" > /dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "       ✓ Tarball verified"
else
    echo "ERROR: Tarball verification failed"
    exit 1
fi

# Clean up temporary directory
rm -rf "$TEMP_DIR"

# Show summary
echo ""
echo "========================================="
echo "           Package Summary               "
echo "========================================="
echo "Package file: ${SUBMISSION_NAME}.tar.gz"
echo "Package size: $(du -h ${SUBMISSION_NAME}.tar.gz | cut -f1)"
echo ""
echo "Contents:"
tar -tzf "${SUBMISSION_NAME}.tar.gz" | sed 's/^/  - /'
echo ""
echo "========================================="
echo "              Next Steps                 "
echo "========================================="
echo "1. Verify the package contents:"
echo "   tar -xzf ${SUBMISSION_NAME}.tar.gz"
echo "   cd ${SUBMISSION_NAME}"
echo "   make"
echo "   ./client"
echo ""
echo "2. Upload to NTU COOL:"
echo "   - Go to course page"
echo "   - Navigate to assignment section"
echo "   - Upload: ${SUBMISSION_NAME}.tar.gz"
echo ""
echo "3. Deadline: Friday, November 14, 2025, 23:59:59"
echo ""
echo "========================================="
echo "         Package Ready! ✓                "
echo "========================================="

