pipeline {
    agent {
        docker {
            image 'project-tsurugi/oltp-sandbox'
            label 'docker'
        }
    }
    environment {
        GITHUB_URL = 'https://github.com/project-tsurugi/yugawara'
        GITHUB_CHECKS = 'tsurugi-check'
        BUILD_PARALLEL_NUM="""${sh(
                returnStdout: true,
                script: 'grep processor /proc/cpuinfo | wc -l'
            )}"""
    }
    stages {
        stage ('Prepare env') {
            steps {
                sh '''
                    ssh-keyscan -t rsa github.com > ~/.ssh/known_hosts
                '''
            }
        }
        stage ('checkout master') {
            steps {
                checkout scm
                sh '''
                    git clean -dfx
                    git submodule sync
                    git submodule update --init --recursive
                '''
            }
        }
        stage ('Install fpdecimal in takatori') {
            steps {
                sh '''
                    cd third_party/takatori/third_party/fpdecimal
                    git log -n 1 --format=%H
                    # git clean -dfx
                    mkdir -p build
                    cd build
                    # clean up cache variables from previous build
                    rm -f CMakeCache.txt
                    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=OFF -DBUILD_DOCUMENTS=OFF -DCMAKE_INSTALL_PREFIX=${WORKSPACE}/.local ..
                    make all install -j${BUILD_PARALLEL_NUM}
                '''
            }
        }
        stage ('Install takatori') {
            steps {
                sh '''
                    cd third_party/takatori
                    git log -n 1 --format=%H
                    # git clean -dfx
                    mkdir -p build
                    cd build
                    # clean up cache variables from previous build
                    rm -f CMakeCache.txt
                    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=OFF -DBUILD_DOCUMENTS=OFF -DCMAKE_INSTALL_PREFIX=${WORKSPACE}/.local -DCMAKE_PREFIX_PATH=${WORKSPACE}/.local ..
                    make all install -j${BUILD_PARALLEL_NUM}
                '''
            }
        }
        stage ('Build') {
            steps {
                sh '''
                    mkdir build
                    cd build
                    cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH=${WORKSPACE}/.local ..
                    make all -j${BUILD_PARALLEL_NUM}
                '''
            }
        }
        stage ('Test') {
            environment {
                GTEST_OUTPUT="xml"
                ASAN_OPTIONS="detect_stack_use_after_return=true"
            }
            steps {
                sh '''
                    cd build
                    make test ARGS="--verbose"
                '''
            }
        }
        stage ('Doc') {
            steps {
                sh '''
                    cd build
                    make doxygen > doxygen.log 2>&1
                    zip -q -r yugawara-doxygen doxygen/html
                '''
            }
        }
        stage ('Coverage') {
            environment {
                GCOVR_COMMON_OPTION='-e ../third_party/ -e ../.*/test.*'
                BUILD_PARALLEL_NUM=4
            }
            steps {
                sh '''
                    cd build
                    mkdir gcovr-xml gcovr-html
                    gcovr -j ${BUILD_PARALLEL_NUM} -r .. --xml ${GCOVR_COMMON_OPTION} -o gcovr-xml/yugawara-gcovr.xml
                    gcovr -j ${BUILD_PARALLEL_NUM} -r .. --html --html-details --html-title "yugawara coverage" ${GCOVR_COMMON_OPTION} -o gcovr-html/yugawara-gcovr.html
                    zip -q -r yugawara-coverage-report gcovr-html
                '''
            }
        }
        stage ('Lint') {
            steps {
                sh '''#!/bin/bash
                    python tools/bin/run-clang-tidy.py -clang-tidy-binary clang-tidy-8 -quiet -export-fixes=build/clang-tidy-fix.yaml -p build -extra-arg=-Wno-unknown-warning-option -header-filter=$(pwd)'/(include|src)/.*\\.h$' $(pwd)'/src/.*' > build/clang-tidy.log 2> build/clang-tidy-error.log
                '''
            }
        }
    }
    post {
        always {
            xunit tools: ([GoogleTest(pattern: '**/*_gtest_result.xml', deleteOutputFiles: false, failIfNotNew: false, skipNoTestFiles: true, stopProcessingIfError: true)]), reduceLog: false
            recordIssues tool: clangTidy(pattern: 'build/clang-tidy.log'),
                qualityGates: [[threshold: 1, type: 'TOTAL', unstable: true]]
            recordIssues tool: gcc4(),
                enabledForFailure: true
            recordIssues tool: doxygen(pattern: 'build/doxygen.log')
            recordIssues tool: taskScanner(
                highTags: 'FIXME', normalTags: 'TODO',
                includePattern: '**/*.md,**/*.txt,**/*.in,**/*.cmake,**/*.cpp,**/*.h',
                excludePattern: 'third_party/**'),
                enabledForFailure: true
            publishCoverage adapters: [coberturaAdapter('build/gcovr-xml/yugawara-gcovr.xml')], sourceFileResolver: sourceFiles('STORE_ALL_BUILD')
            archiveArtifacts allowEmptyArchive: true, artifacts: 'build/yugawara-coverage-report.zip, build/yugawara-doxygen.zip, build/clang-tidy.log, build/clang-tidy-fix.yaml', onlyIfSuccessful: true
            notifySlack('tsurugi-dev')
        }
    }
}
