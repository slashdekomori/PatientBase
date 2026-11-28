import Constants
from docx import Document
from docxtpl import DocxTemplate


def ParseForm045(data):
    if data["AIGHumanOrAnimal"] == "лошадиный":
        return DocxTemplate(Constants.PATH_TO_FORM045_HORSE)
    elif data["AIGHumanOrAnimal"] == "человеческий":
        return DocxTemplate(Constants.PATH_TO_FORM045_HUMAN)
    else:
        return False
