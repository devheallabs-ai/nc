.PHONY: build test publish publish-test clean install dev lint

# Build distribution packages
build: clean
	python -m build

# Install locally in development mode
dev:
	pip install -e ".[dev]"

# Install from built package
install:
	pip install dist/nc_lang-*.whl

# Run tests
test:
	python -m pytest tests/ -v

# Lint
lint:
	python -m ruff check nc_lang/
	python -m mypy nc_lang/

# Publish to PyPI
publish: build
	python -m twine upload dist/*

# Publish to Test PyPI
publish-test: build
	python -m twine upload --repository testpypi dist/*

# Clean build artifacts
clean:
	rm -rf build/ dist/ *.egg-info nc_lang/*.egg-info
	find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true
