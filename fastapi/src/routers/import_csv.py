from fastapi import HTTPException, APIRouter
from pathlib import Path
from pydantic import ValidationError
from csv import DictReader
from src.data.init import SessionDep
from src.model.subject import Subject, SubjectCreate
from src.model.lesson import Lesson, LessonCreate


router = APIRouter( prefix="/school", tags=["school"] )

SUBJ_CSV_PATH = Path("/Users/lily/school/fastapi/src/assets/subjects.csv") 
LESSON_CSV_PATH = Path("/Users/lily/school/fastapi/src/assets/lessons.csv") 

async def import_from_csv(path: Path, create_model, db_model, session: SessionDep):
    
    if not path.exists():
        raise HTTPException(status_code=404, detail="Default CSV file not found")

    created = 0
    errors: list[dict] = []

    with path.open("r", encoding="utf-8") as f:
        reader = DictReader(f)

        for line_no, row in enumerate(reader, start=2):  # start=2: первая строка — заголовки
            try:
                obj_in = create_model.model_validate(row)

            except ValidationError as e:
                errors.append(
                    {
                        "line": line_no,
                        "row": row,
                        "errors": e.errors(),
                    }
                )
                continue

            obj = db_model(**obj_in.model_dump())
            session.add(obj)
            created += 1

    return created, errors

@router.post("/import-data")
async def import_refs(session: SessionDep):
    
    subj_created, subj_errors = await import_from_csv(
        path=SUBJ_CSV_PATH,
        create_model=SubjectCreate,
        db_model=Subject,
        session=session,
    )

    lesson_created, lesson_errors = await import_from_csv(
        path=LESSON_CSV_PATH,
        create_model=LessonCreate,
        db_model=Lesson,
        session=session,
    )

    # bank_created, bank_errors = await import_from_csv(
    #     path=BANK_CSV_PATH,
    #     create_model=BankCreate,
    #     db_model=Bank,
    #     session=session,
    # )

    session.commit()
    return {
        "status": "ok",
        "customers": {
            "created": subj_created,
            "errors": subj_errors,
        },
        "currency": {
            "created": lesson_created,
            "errors": lesson_errors,
        },
        # "bank": {
        #     "created": bank_created,
        #     "errors": bank_errors,
        # },
    }