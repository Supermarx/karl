#pragma once

#include <map>

#include <rusql/database.hpp>

#include <supermarx/product.hpp>

namespace supermarx {
    struct RuSQLService {
        template <typename ... Args>
        RuSQLService(std::string const& embedded_dir)
        {
            const char *server_options[] = \
                {"test", "--innodb=OFF", "-h", embedded_dir.c_str(), NULL};
            int num_options = (sizeof(server_options)/sizeof(char*)) - 1;
            mysql_library_init(num_options, const_cast<char**>(server_options), NULL);

            {
                auto db = std::make_shared<rusql::Database>(rusql::Database::ConstructionInfo());
                auto result = db->query("show databases");
                bool found_karl = false;
                while(result){
                    if(result.get<std::string>("Database") == "karl"){
                        found_karl = true;
                    }
                    result.next();
                }
                if(!found_karl){
                    db->execute("create database karl");
                }
            }

            database = std::make_shared<rusql::Database>(rusql::Database::ConstructionInfo("karl"));

            update_database_schema();
            prepare_statements();
        }

        void add_product(Product const& p){
            prepared_statements.at(Statement::AddProduct)->execute(p.name, p.price_in_cents);
        }

        std::vector<Product> get_products_by_name(std::string const& name){
            rusql::PreparedStatement& query = *prepared_statements.at(Statement::GetProductByName);
            query.execute(name);

            std::vector<Product> products;
            Product row;
            query.bind_results(row.name, row.price_in_cents);
            while(query.fetch()){
                products.push_back(row);
            }

            return products;
        }

        ~RuSQLService() {
            prepared_statements.clear();
            database.reset();
            mysql_library_end();
        }

    private:
        //! Makes sure the right schema is there.
        // This function is called after the connection to the database is made,
        // and can be used to update tables to a new version, that kind of stuff.
        void update_database_schema(){
            bool products_found = false;
            auto result = database->query("show tables");
            while(result){
                if(result.get<std::string>(0) == "products"){
                    products_found = true;
                    break;
                }
                result.next();
            }

            if(!products_found){
                database->execute("create table products (product_name varchar(1024), price_in_cents decimal(10,2))");
            }
        }

        void prepare_statements() {
            auto create_statement =
            [&](Statement const& s, std::string const& query){
                prepared_statements[s] =
                std::make_shared<rusql::PreparedStatement>(database->prepare(query));
            };

            {
                create_statement(Statement::AddProduct, "insert into products (product_name, price_in_cents) values(?, ?)");
                create_statement(Statement::GetProductByName, "select * from products where product_name = ?");
            }
        }

        std::shared_ptr<rusql::Database> database;

        enum class Statement {
            AddProduct,
            GetProductByName
        };

        std::map<Statement, std::shared_ptr<rusql::PreparedStatement>> prepared_statements;
    };
}
