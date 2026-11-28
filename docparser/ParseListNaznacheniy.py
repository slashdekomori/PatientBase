import Constants
from docx import Document
from docxtpl import DocxTemplate


def ParseAppointmentList(data):
    ShablonAppointmentsList = DocxTemplate(Constants.PATH_TO_APPOINTMENTS_LIST)

    return ShablonAppointmentsList
