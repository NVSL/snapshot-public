.PHONY: check_compiler
check_compiler:
	@$(MAKE) -s .so_deps PERFORM_CHECKS=0
	@printf "Checking for C++20 support... "
	@if echo "int main() {}" | $(CXX) -std=c++20 -x c++ -o /dev/null - >/dev/null 2>&1; then \
		printf "found\n"; \
	else \
		printf "not found\n"; \
	fi

.so_deps:
	@$(MAKE) -s check_so_libuuid PERFORM_CHECKS=0
	@$(MAKE) -s check_so_libboost_system PERFORM_CHECKS=0
	@touch .so_deps

check_so_%:
	@printf "%s %s... " "Looking for" $(@:check_so_%=%.so)
	@if ldconfig -p | grep -q  $(@:check_so_%=%.so); then \
		printf "found\n";\
	else\
		printf "not found.\n\nInstall  $(@:check_so_%=%.so)\n";\
		exit 1;\
	fi
