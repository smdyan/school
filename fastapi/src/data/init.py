from sqlmodel import create_engine, SQLModel, Session
from typing import Annotated
from fastapi import Depends


sqlite_file_name = "school.db"
sqlite_url = f"sqlite:///./database/{sqlite_file_name}"
connect_args = {"check_same_thread": False}
# turn on alchemy 2.0 futures
engine = create_engine( sqlite_url, echo=True, future=True )


async def create_db_and_tables():
    SQLModel.metadata.create_all( engine )


def get_session():
# same as creating manualu session = Session(engine), but no need to session.close()
    with Session( engine ) as session:
        yield session


SessionDep = Annotated[Session, Depends( get_session )]
