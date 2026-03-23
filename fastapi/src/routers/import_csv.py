from fastapi import HTTPException, APIRouter
from pathlib import Path
from pydantic import ValidationError
from sqlmodel import select
from csv import DictReader
from src.data.init import SessionDep
from src.model.subject import Subject, SubjectCreate
from src.model.lesson import Lesson, LessonCreate
from src.model.weekday import Weekday, WeekdayCreate


router = APIRouter( prefix="/school", tags=["school"] )

WEEKDAYS_CSV_PATH = Path("/Users/lily/school/fastapi/src/assets/weekdays.csv") 
LESSONS_CSV_PATH = Path("/Users/lily/school/fastapi/src/assets/lessons.csv") 

async def _get_or_create_subject_id(session, subjectName: str) -> int:
    name = subjectName.strip()
    res = session.execute(select(Subject).where(Subject.name == name))
    obj = res.scalar_one_or_none()
    if obj:
        return obj.id

    obj = Subject(name=name)
    session.add(obj)
    session.flush()
    return obj.id

async def _get_weekday_id(session, dayNum: int) -> int:
    res = session.execute(select(Weekday).where(Weekday.dayNum == dayNum))
    obj = res.scalar_one_or_none()
    return obj.id


async def import_lessons_from_csv(path: Path, session):
    if not path.exists():
        raise HTTPException(status_code=404, detail="Default CSV file not found")

    created = 0
    errors: list[dict] = []

    subject_cache: dict[str, int] = {}
    weekday_cache: dict[int, int] = {}

    with path.open("r", encoding="utf-8") as f:
        reader = DictReader(f)

        for line_no, row in enumerate(reader, start=2):
            try:
                obj_in = LessonCreate.model_validate(row)
            except ValidationError as e:
                errors.append({"line": line_no, "row": row, "errors": e.errors()})
                continue

            data = obj_in.model_dump()

            # 1) берём name, удаляем его из данных
            subjectName = (data.pop("subjectName", "") or "").strip()
            if not subjectName:
                errors.append({"line": line_no, "row": row, "errors": [{"msg": "subjectName is empty"}]})
                continue

            # 2) получаем name_id через БД (с кэшем)
            if subjectName in subject_cache:
                subjectId = subject_cache[subjectName]
            else:
                subjectId = await _get_or_create_subject_id(session, subjectName)
                subject_cache[subjectName] = subjectId

            # 3) подставляем name_id и создаём ORM объект
            data["subjectId"] = subjectId
            
            # тоже для weekdayId
            dayNum = data.pop("dayNum")
            if not dayNum:
                errors.append({"line": line_no, "row": row, "errors": [{"msg": "dayNum is empty"}]})
                continue

            if dayNum in weekday_cache:
                weekdayId = weekday_cache[dayNum]
            else:
                weekdayId = await _get_weekday_id(session, dayNum)
                weekday_cache[dayNum] = weekdayId

            data["weekdayId"] = weekdayId
            
            obj = Lesson(**data)

            session.add(obj)
            created += 1

    return created, errors


async def import_weekdays_from_csv(path: Path, session: SessionDep):
    
    if not path.exists():
        raise HTTPException(status_code=404, detail="Default CSV file not found")

    created = 0
    errors: list[dict] = []

    with path.open("r", encoding="utf-8") as f:
        reader = DictReader(f)

        for line_no, row in enumerate(reader, start=2):  # start=2: первая строка — заголовки
            try:
                obj_in = WeekdayCreate.model_validate(row)

            except ValidationError as e:
                errors.append(
                    {
                        "line": line_no,
                        "row": row,
                        "errors": e.errors(),
                    }
                )
                continue

            obj = Weekday(**obj_in.model_dump())
            session.add(obj)
            created += 1

    return created, errors



@router.post("/import-data")
async def import_refs(session: SessionDep):
    
    weekdays_created, weekdays_errors = await import_weekdays_from_csv(
        path=WEEKDAYS_CSV_PATH,
        session=session,
    )
    session.commit()

    lesson_created, lesson_errors = await import_lessons_from_csv(
        path=LESSONS_CSV_PATH,
        session=session,
    )
    session.commit()
    
    return {
        "status": "ok",
        "weekdays": {
            "created": weekdays_created,
            "errors": weekdays_errors,
        },
        "lessons": {
            "created": lesson_created,
            "errors": lesson_errors,
        },

    }