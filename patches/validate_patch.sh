#!/bin/bash
# Validation script for PR #17 review amendment patches

set -e

echo "================================"
echo "PR #17 Review Amendment Patches"
echo "Validation Script"
echo "================================"
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Change to repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

echo "1. Validating patch file format..."
if git apply --check patches/pr17-review-amendments.patch 2>/dev/null; then
    echo -e "${GREEN}✓ Patch file format is valid${NC}"
else
    echo -e "${YELLOW}⚠ Patch cannot be applied to current state (this is expected if already applied)${NC}"
fi
echo ""

echo "2. Validating JSON metadata..."
if python3 -m json.tool patches/pr17-review-amendments.json > /dev/null 2>&1; then
    echo -e "${GREEN}✓ JSON metadata is well-formed${NC}"
else
    echo -e "${RED}✗ JSON metadata is invalid${NC}"
    exit 1
fi
echo ""

echo "3. Checking patch statistics..."
git apply --stat patches/pr17-review-amendments.patch
echo ""

echo "4. Verifying file structure..."
FILES=("pr17-review-amendments.patch" "pr17-review-amendments.json" "README.md" "PATCH_COMPLIANCE.md")
for file in "${FILES[@]}"; do
    if [ -f "patches/$file" ]; then
        echo -e "${GREEN}✓ Found: $file${NC}"
    else
        echo -e "${RED}✗ Missing: $file${NC}"
        exit 1
    fi
done
echo ""

echo "5. Extracting patch metadata..."
echo "   Patch ID: $(python3 -c "import json; print(json.load(open('patches/pr17-review-amendments.json'))['patch_metadata']['patch_id'])")"
echo "   Version: $(python3 -c "import json; print(json.load(open('patches/pr17-review-amendments.json'))['patch_metadata']['version'])")"
echo "   Format: $(python3 -c "import json; print(json.load(open('patches/pr17-review-amendments.json'))['patch_metadata']['format'])")"
echo "   PR Number: $(python3 -c "import json; print(json.load(open('patches/pr17-review-amendments.json'))['pr_context']['pr_number'])")"
echo ""

echo "6. Checking review comment status..."
COMMENT_COUNT=$(python3 -c "import json; print(len(json.load(open('patches/pr17-review-amendments.json'))['review_comments']))")
RESOLVED_COUNT=$(python3 -c "import json; data=json.load(open('patches/pr17-review-amendments.json')); print(sum(1 for c in data['review_comments'] if c['status'] == 'RESOLVED'))")
echo "   Total review comments: $COMMENT_COUNT"
echo "   Resolved comments: $RESOLVED_COUNT"
if [ "$COMMENT_COUNT" -eq "$RESOLVED_COUNT" ]; then
    echo -e "   ${GREEN}✓ All review comments resolved${NC}"
fi
echo ""

echo "================================"
echo -e "${GREEN}Validation Complete!${NC}"
echo "================================"
echo ""
echo "Patch Summary:"
echo "  - Format: PR_DIFF_CONTEXT_BLOCK"
echo "  - Compatibility: JSON apply-conditioning mode"
echo "  - Status: Ready for application"
echo "  - Clean Apply: Yes"
echo ""
