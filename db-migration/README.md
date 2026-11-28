1. Скопировать папку `db-migration/` туда где есть доступ к новой базе
2. Перейти в папку `db-migration/`
3. Поместить файл `Database.db` в папку `db-migration/`
4. Запустить команду:
python3 sqlite_to_csv.py DataBase.db
5. Запустить команду:
PGPASSWORD='<пароль>' PGSSLMODE=require psql -h <IP-сервера> -U pb -d <Название-базы> -f load_cases.sql

Для добавления хранилища DOCX в уже существующую базу отделения:
PGPASSWORD='<пароль>' PGSSLMODE=require psql -h <IP-сервера> -U pb -d <Название-базы> -f case_documents.sql

Для хранения настроек отделения (DepartmentInfo.json) на сервере:
PGPASSWORD='<пароль>' PGSSLMODE=require psql -h <IP-сервера> -U pb -d <Название-базы> -f department_info.sql

Загрузить готовый DepartmentInfo.json в базу отделения:
PGPASSWORD='<пароль>' PGSSLMODE=require python3 load_department_info.py DepartmentInfo.json -h <IP-сервера> -U pb -d <Название-базы>

Клиент при каждом запуске сам синхронизирует настройки с сервером: если в базе пусто, а локальный файл есть — загружает его; если локальный файл правился без связи с сервером — загружает локальные изменения. Окно настроек записывает изменения в базу, а при недоступности сервера — только локально.
