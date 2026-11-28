import common
from docxtpl import DocxTemplate
from datetime import timedelta
import datetime
import Constants


def ParsePervichniyOsmotr(data):
    ShablonPervichnogoOsmotra = DocxTemplate(
        Constants.PATH_TO_PERVICHNIY_OSMOTR_TEMPLATE
    )

    common.parse_random_vital(data)
    common.parse_obsledovaniya_do_postupleniya(data)
    first_examination_with_time = common.parse_date_of_first_examination(data)
    common.parse_current_naznacheniya(data, first_examination_with_time)
    common.parse_manipulation_to_current_date(
        data, first_examination_with_time, ShablonPervichnogoOsmotra
    )

    return ShablonPervichnogoOsmotra
