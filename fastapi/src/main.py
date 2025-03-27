from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, Query
from sqlmodel import Field, Session, SQLModel, create_engine, select
from fastapi.middleware.cors import CORSMiddleware

class Schedule(SQLModel, table=True):
    id: int = Field(primary_key=True)
    weekday: str = Field(index=True)
    lesson1: str = Field(default=None)
    lesson2: str = Field(default=None)
    lesson3: str = Field(default=None)
    lesson4: str = Field(default=None)
    lesson5: str = Field(default=None)
    lesson6: str = Field(default=None)
    

sqlite_file_name = "db/data.db"
sqlite_url = f"sqlite:///{sqlite_file_name}"

connect_args = {"check_same_thread": False}
engine = create_engine(sqlite_url, connect_args=connect_args)

def create_db_and_tables():
    SQLModel.metadata.create_all(engine)

def get_session():
    with Session(engine) as session:
        yield session

SessionDep = Annotated[Session, Depends(get_session)]

app = FastAPI()

origins = ["*"]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.on_event("startup")
def on_startup():
    create_db_and_tables()


@app.get("/")
def read_root():
    return {"Game": "World"}


@app.post("/sched/")
def create_day(schedule: Schedule, session: SessionDep) -> Schedule:
    session.add(schedule)
    session.commit()
    session.refresh(schedule)
    return schedule


@app.get("/sched/")
def read_week(
    session: SessionDep,
    offset: int = 0,
    limit: Annotated[int, Query(le=100)] = 100,
) -> list[Schedule]:
    week = session.exec(select(Schedule).offset(offset).limit(limit)).all()
    return week


@app.get("/sched/{id}")
def read_day(id: int, session: SessionDep) -> Schedule:
    day = session.get(Schedule, id)
    if not day:
        raise HTTPException(status_code=404, detail="schedule not found")
    return day


@app.delete("/sched/{id}")
def delete_day(id: int, session: SessionDep):
    day = session.get(Schedule, id)
    if not day:
        raise HTTPException(status_code=404, detail="schedule not found")
    session.delete(day)
    session.commit()
    return {"ok": True}