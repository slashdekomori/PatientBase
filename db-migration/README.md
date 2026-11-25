1. Скопировать папку `db-migration/` туда где есть доступ к новой базе
2. Перейти в папку `db-migration/`
3. Поместить файл `Database.db` в папку `db-migration/`
4. Запустить команду:
python3 sqlite_to_csv.py DataBase.db
5. Запустить команду:
PGPASSWORD='<пароль>' PGSSLMODE=require psql -h <IP-сервера> -U pb -d <Название-базы> -f load_cases.sql

Для добавления хранилища DOCX в уже существующую базу отделения выполнить:
PGPASSWORD='<пароль>' PGSSLMODE=require psql -h <IP-сервера> -U pb -d <Название-базы> -f case_documents.sql
