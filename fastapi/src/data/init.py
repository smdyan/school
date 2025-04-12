from sqlmodel import create_engine, SQLModel, Session
from typing import Annotated
from fastapi import Depends


sqlite_file_name = "school.db"
sqlite_url = f"sqlite:///./database/{sqlite_file_name}"
connect_args = {"check_same_thread": False}
engine = create_engine( sqlite_url, echo=True, future=True )


async def init_db():
    SQLModel.metadata.create_all( engine )


def get_session():
    with Session( engine ) as session:
        yield session


SessionDep = Annotated[Session, Depends( get_session )]
