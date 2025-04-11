# main.py create and export app

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from src.routes.schedule import router
from src.data.init import init_db

async def Lifespan(app: FastAPI):
    await init_db()
    print("game")
    yield
    print("over")


app = FastAPI( lifespan=Lifespan )
app.include_router( router )

origins = ["*"]
app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)
