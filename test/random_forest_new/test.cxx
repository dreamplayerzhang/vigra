/************************************************************************/
/*                                                                      */
/*        Copyright 2014-2015 by Ullrich Koethe and Philip Schill       */
/*                                                                      */
/*    This file is part of the VIGRA computer vision library.           */
/*    The VIGRA Website is                                              */
/*        http://hci.iwr.uni-heidelberg.de/vigra/                       */
/*    Please direct questions, bug reports, and contributions to        */
/*        ullrich.koethe@iwr.uni-heidelberg.de    or                    */
/*        vigra@informatik.uni-hamburg.de                               */
/*                                                                      */
/*    Permission is hereby granted, free of charge, to any person       */
/*    obtaining a copy of this software and associated documentation    */
/*    files (the "Software"), to deal in the Software without           */
/*    restriction, including without limitation the rights to use,      */
/*    copy, modify, merge, publish, distribute, sublicense, and/or      */
/*    sell copies of the Software, and to permit persons to whom the    */
/*    Software is furnished to do so, subject to the following          */
/*    conditions:                                                       */
/*                                                                      */
/*    The above copyright notice and this permission notice shall be    */
/*    included in all copies or substantial portions of the             */
/*    Software.                                                         */
/*                                                                      */
/*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND    */
/*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES   */
/*    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND          */
/*    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT       */
/*    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,      */
/*    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING      */
/*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR     */
/*    OTHER DEALINGS IN THE SOFTWARE.                                   */
/*                                                                      */
/************************************************************************/
#include <vigra/unittest.hxx>
#include <vigra/random_forest_new.hxx>
#ifdef HasHDF5
  #include <vigra/hdf5impex.hxx>
#endif
//#include <vigra/algorithm.hxx>
#include <vigra/random.hxx>

using namespace vigra;

struct RandomForestTests
{
    void test_base_class()
    {
        typedef BinaryForest Graph;
        typedef Graph::Node Node;
        typedef LessEqualSplitTest<double> SplitTest;
        typedef ArgMaxAcc Acc;
        typedef RandomForest<MultiArray<2, double>, MultiArray<1, int>, SplitTest, Acc> RF;

        // Build a forest from scratch.
        Graph gr;
        PropertyMap<Node, SplitTest> split_tests;
        PropertyMap<Node, size_t> leaf_responses;
        {
            Node n0 = gr.addNode();
            Node n1 = gr.addNode();
            Node n2 = gr.addNode();
            Node n3 = gr.addNode();
            Node n4 = gr.addNode();
            Node n5 = gr.addNode();
            Node n6 = gr.addNode();
            gr.addArc(n0, n1);
            gr.addArc(n0, n2);
            gr.addArc(n1, n3);
            gr.addArc(n1, n4);
            gr.addArc(n2, n5);
            gr.addArc(n2, n6);

            split_tests.insert(n0, SplitTest(0, 0.6));
            split_tests.insert(n1, SplitTest(1, 0.25));
            split_tests.insert(n2, SplitTest(1, 0.75));
            leaf_responses.insert(n3, 0);
            leaf_responses.insert(n3, 0);
            leaf_responses.insert(n4, 1);
            leaf_responses.insert(n5, 2);
            leaf_responses.insert(n6, 3);
        }
        std::vector<int> distinct_labels;
        distinct_labels.push_back(0);
        distinct_labels.push_back(1);
        distinct_labels.push_back(-7);
        distinct_labels.push_back(3);
        auto const pspec = ProblemSpecNew<int>().num_features(2).distinct_classes(distinct_labels);
        RF rf = RF(gr, split_tests, leaf_responses, pspec);

        // Check if the given points are predicted correctly.
        double test_x_values[] = {
            0.2, 0.4, 0.2, 0.4, 0.7, 0.8, 0.7, 0.8,
            0.2, 0.2, 0.7, 0.7, 0.2, 0.2, 0.8, 0.8
        };
        MultiArray<2, double> test_x(Shape2(8, 2), test_x_values);
        int test_y_values[] = {
            0, 0, 1, 1, -7, -7, 3, 3
        };
        MultiArray<1, int> test_y(Shape1(8), test_y_values);
        MultiArray<1, int> pred_y(Shape1(8));
        rf.predict(test_x, pred_y, 1);
        shouldEqualSequence(pred_y.begin(), pred_y.end(), test_y.begin());
    }

    void test_default_rf()
    {
        typedef MultiArray<2, double> Features;
        typedef MultiArray<1, int> Labels;

        double train_x_values[] = {
            0.2, 0.4, 0.2, 0.4, 0.7, 0.8, 0.7, 0.8,
            0.2, 0.2, 0.7, 0.7, 0.2, 0.2, 0.8, 0.8
        };
        Features train_x(Shape2(8, 2), train_x_values);
        int train_y_values[] = {
            0, 0, 1, 1, -7, -7, 3, 3
        };
        Labels train_y(Shape1(8), train_y_values);
        Features test_x(train_x);
        Labels test_y(train_y);

        std::vector<RandomForestOptionTags> splits;
        splits.push_back(RF_GINI);
        splits.push_back(RF_ENTROPY);
        splits.push_back(RF_KSD);
        for (auto split : splits)
        {
            RandomForestNewOptions const options = RandomForestNewOptions()
                                                       .tree_count(1)
                                                       .bootstrap_sampling(false)
                                                       .split(split)
                                                       .n_threads(1);
            auto rf = random_forest(train_x, train_y, options);
            Labels pred_y(test_y.shape());
            rf.predict(test_x, pred_y, 1);
            shouldEqualSequence(pred_y.begin(), pred_y.end(), test_y.begin());
        }
    }

    void test_oob_visitor()
    {
        // Create a (noisy) grid with datapoints and assign classes as in a 4x4 chessboard.
        size_t const nx = 100;
        size_t const ny = 100;

        RandomNumberGenerator<MersenneTwister> rand;
        MultiArray<2, double> train_x(Shape2(nx*ny, 2));
        MultiArray<1, int> train_y(Shape1(nx*ny));
        for (size_t y = 0; y < ny; ++y)
        {
            for (size_t x = 0; x < nx; ++x)
            {
                train_x(y*nx+x, 0) = x + 2*rand.uniform()-1;
                train_x(y*nx+x, 1) = y + 2*rand.uniform()-1;
                if ((x/25+y/25) % 2 == 0)
                    train_y(y*nx+x) = 0;
                else
                    train_y(y*nx+x) = 1;
            }
        }

        RandomForestNewOptions const options = RandomForestNewOptions()
                                                   .tree_count(10)
                                                   .bootstrap_sampling(true)
                                                   .n_threads(1);
        OOBError oob;
        auto rf = random_forest(train_x, train_y, options, create_visitor(oob));



    }

#ifdef HasHDF5
    void test_rf_mnist()
    {
        typedef MultiArray<2, UInt8> Features;
        typedef MultiArray<1, UInt8> Labels;

        std::string train_filename = "/home/philip/data/mnist/mnist_train_reshaped.h5";
        std::string test_filename = "/home/philip/data/mnist/mnist_test_reshaped.h5";

        Features train_x, test_x;
        Labels train_y, test_y;
        HDF5File train_file(train_filename.c_str(), HDF5File::ReadOnly);
        train_file.readAndResize("images", train_x);
        train_file.readAndResize("labels", train_y);
        train_file.close();
        HDF5File test_file(test_filename.c_str(), HDF5File::ReadOnly);
        test_file.readAndResize("images", test_x);
        test_file.readAndResize("labels", test_y);
        test_file.close();
        train_x = train_x.transpose();
        test_x = test_x.transpose();
        should(train_x.shape()[0] == train_y.size());
        should(test_x.shape()[0] == test_y.size());
        should(train_x.shape()[1] == test_x.shape()[1]);

        std::vector<RandomForestOptionTags> splits = {RF_GINI};//, RF_ENTROPY, RF_KSD};
        for (auto split : splits)
        {
            RandomForestNewOptions const options = RandomForestNewOptions()
                                                       .tree_count(10)
                                                       .split(split)
                                                       .n_threads(1);
            auto rf = random_forest(train_x, train_y, options);

            // Predict using all trees.
            Labels pred_y(test_y.shape());
            rf.predict(test_x, pred_y, 1);
            size_t count = 0;
            for (size_t i = 0; i < test_y.size(); ++i)
                if (pred_y(i) == test_y(i))
                    ++count;
            double performance = count / static_cast<double>(test_y.size());
            std::cout << "performance: " << performance << std::endl;
        }
    }
#endif
};

struct RandomForestTestSuite : public test_suite
{
    RandomForestTestSuite()
        :
        test_suite("RandomForest test")
    {
        add(testCase(&RandomForestTests::test_base_class));
        add(testCase(&RandomForestTests::test_default_rf));
        add(testCase(&RandomForestTests::test_oob_visitor));
#ifdef HasHDF5
        add(testCase(&RandomForestTests::test_rf_mnist));
#endif
    }
};

int main(int argc, char** argv)
{
    RandomForestTestSuite forest_test;
    int failed = forest_test.run(testsToBeExecuted(argc, argv));
    std::cout << forest_test.report() << std::endl;
    return (failed != 0);
}