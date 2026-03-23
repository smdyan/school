from fastapi import APIRouter
from src.routers import import_csv
from src.routers import schedule


def get_router():
    router = APIRouter()
    router.include_router( import_csv.router )
    router.include_router( schedule.router )
    return router