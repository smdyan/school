from typing import Annotated
from fastapi import HTTPException, Query, APIRouter
from sqlmodel import select
from src.data.init import SessionDep
from src.model.schedule import Schedule, ScheduleCreate


router = APIRouter( prefix="/sch" )


@router.get("/")
def get_week(
    session: SessionDep,
    offset: int = 0,
    limit: Annotated[int, Query(le=100)] = 100,
    ) -> list[Schedule]:
    # result = session.exec(select(Schedule))
    result = session.exec(select(Schedule).offset(offset).limit(limit)).all()
    # schedules = result.scalars().all()
    return [
        Schedule(day=schedule.day,
                 lesson=schedule.lesson,
                 id=schedule.id) for schedule in result
    ]


@router.post("/")
def add_schedule( schedule: ScheduleCreate, session: SessionDep ) -> Schedule:
    schedule = Schedule( day=schedule.day, lesson=schedule.lesson )
    session.add( schedule )
    session.commit()
    session.refresh( schedule )
    return schedule


@router.get("/{id}")
async def get_day(id: int, session: SessionDep) -> Schedule:
    schedule = session.get(Schedule, id)
    if not schedule:
        raise HTTPException(status_code=404, detail="schedule not found")
    return schedule


@router.delete("/{id}")
async def delete_day(id: int, session: SessionDep):
    schedule = session.get(Schedule, id)
    if not schedule:
        raise HTTPException(status_code=404, detail="schedule not found")
    session.delete(schedule)
    session.commit()
    return {"ok": True}
