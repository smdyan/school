## venv
pip install -r requirements.txt
## FAPI
run app in venv
```code
source .venv/bin/activate
uvicorn src.main:app --reload
```